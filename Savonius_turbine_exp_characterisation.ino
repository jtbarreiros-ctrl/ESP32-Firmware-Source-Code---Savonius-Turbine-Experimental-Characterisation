#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HX711.h>


// ============================================================================
// EXPERIMENTAL CONFIGURATION
// ============================================================================

// Select the experimental medium by uncommenting ONE of the following lines:
#define OPEN_WATER_TEST
//#define WIND_TUNNEL_TEST


// --- Turbine Geometry ---
const float TURBINE_HEIGHT = 0.230;        // Rotor height [m]
const float TURBINE_WIDTH  = 0.244;        // Rotor projected width / diameter [m]
const float TURBINE_RADIUS = TURBINE_WIDTH / 2.0;

const float SWEPT_AREA = TURBINE_HEIGHT * TURBINE_WIDTH;


// --- Prony Brake Geometry ---
const float LEVER_ARM = 0.1;             // Effective lever arm [m]


// --- Experimental Flow Condition & Fluid Properties (MIT Tables) ---
// Change FLOW_VELOCITY before each test condition.
#ifdef OPEN_WATER_TEST
const float FLOW_VELOCITY       = 1.0;       // Open-water tests: 1.0, 1.5 or 2.0 m/s
const float FLUID_DENSITY       = 1025.3;    // Seawater density at 18 C, S = 35 g/kg [kg/m^3]
const float KINEMATIC_VISCOSITY = 1.12e-6;   // Seawater kinematic viscosity at 18 C [m^2/s]
#endif

#ifdef WIND_TUNNEL_TEST
const float FLOW_VELOCITY       = 8.0;       // Wind-tunnel test: 8.0, 10.0 or 12.0 m/s
const float FLUID_DENSITY       = 1.170;     // Dry air density at 25 C [kg/m^3]
const float KINEMATIC_VISCOSITY = 1.573e-5;  // Dry air kinematic viscosity at 25 C [m^2/s]
#endif


// --- Load-Cell Calibration ---
const float LOAD_CELL_FACTOR_WATER = 205.93; // 10 kg load cell
const float LOAD_CELL_FACTOR_AIR   = 953.67; // 1 kg load cell


// ============================================================================
// PIN ASSIGNMENTS
// ============================================================================

const int HALL_PIN = 5;
const int TEMPERATURE_PIN = 4;

const int HX711_DT_PIN = 16;
const int HX711_SCK_PIN = 17;

const int GREEN_STATUS_LED_PIN = 19;


// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================

LiquidCrystal_I2C lcd(0x27, 20, 4);

OneWire oneWire(TEMPERATURE_PIN);
DallasTemperature temperatureSensor(&oneWire);

HX711 loadCell;


// ============================================================================
// ROTATIONAL-SPEED VARIABLES
// ============================================================================

// Two magnets are mounted approximately 180 degrees apart on the rotating shaft,
// producing two pulses per revolution.
const int PULSES_PER_REVOLUTION = 2;

volatile unsigned long lastPulseTimeMicros = 0;
volatile unsigned long pulsePeriodMicros = 0;


// ============================================================================
// ACQUISITION TIMING
// ============================================================================

// Main experimental acquisition: 10 measurements per second
const unsigned long SAMPLE_INTERVAL_MS = 100;       // 10 Hz

// Temperature reading interval (for monitoring/logging only)
const unsigned long TEMPERATURE_INTERVAL_MS = 1000; // 1 Hz

// LCD refresh rate
const unsigned long DISPLAY_INTERVAL_MS = 500;      // 2 Hz

unsigned long previousSampleTime = 0;
unsigned long previousTemperatureTime = 0;
unsigned long previousDisplayTime = 0;


// ============================================================================
// GLOBAL MEASUREMENT VARIABLES
// ============================================================================

float rotationalSpeedRPM = 0.0;
float angularVelocity = 0.0;

float measuredMassGrams = 0.0;
float reactionForce = 0.0;
float torque = 0.0;

float temperatureC = 18.0; // Initial default value
float fluidDensity = FLUID_DENSITY;
float kinematicViscosity = KINEMATIC_VISCOSITY;
float reynoldsNumber = 0.0;

float mechanicalPower = 0.0;
float availablePower = 0.0;

float tipSpeedRatio = 0.0;
float powerCoefficient = 0.0;

bool temperatureSensorOK = false;
bool loadCellOK = false;


// ============================================================================
// HALL-EFFECT SENSOR INTERRUPT
// ============================================================================

void IRAM_ATTR hallPulseISR()
{
    unsigned long currentTimeMicros = micros();
    unsigned long elapsedTime = currentTimeMicros - lastPulseTimeMicros;

    // Ignore pulses separated by less than 10 ms to reject electrical noise.
    if (elapsedTime > 10000)
    {
        pulsePeriodMicros = elapsedTime;
        lastPulseTimeMicros = currentTimeMicros;
    }
}


// ============================================================================
// SETUP
// ============================================================================

void setup()
{
    Serial.begin(115200);

    // ------------------------------------------------------------------------
    // Status LED
    // ------------------------------------------------------------------------
    pinMode(GREEN_STATUS_LED_PIN, OUTPUT);
    digitalWrite(GREEN_STATUS_LED_PIN, LOW);

    // ------------------------------------------------------------------------
    // LCD
    // ------------------------------------------------------------------------
    lcd.begin();
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("Initializing DAQ...");

    // ------------------------------------------------------------------------
    // Hall-Effect Sensor
    // ------------------------------------------------------------------------
    pinMode(HALL_PIN, INPUT_PULLUP);

    attachInterrupt(
        digitalPinToInterrupt(HALL_PIN),
        hallPulseISR,
        FALLING
    );

    // ------------------------------------------------------------------------
    // Temperature Sensor (Monitoring Only)
    // ------------------------------------------------------------------------
    temperatureSensor.begin();

    if (temperatureSensor.getDeviceCount() > 0)
    {
        temperatureSensorOK = true;
        temperatureSensor.setWaitForConversion(false);
        temperatureSensor.requestTemperatures();
    }
    else
    {
        temperatureSensorOK = false;
        Serial.println("WARNING: DS18B20 temperature sensor not detected.");
    }

    // ------------------------------------------------------------------------
    // Load Cell and HX711
    // ------------------------------------------------------------------------
    loadCell.begin(HX711_DT_PIN, HX711_SCK_PIN);

#ifdef OPEN_WATER_TEST
    loadCell.set_scale(LOAD_CELL_FACTOR_WATER);
#endif

#ifdef WIND_TUNNEL_TEST
    loadCell.set_scale(LOAD_CELL_FACTOR_AIR);
#endif

    lcd.setCursor(0, 1);
    lcd.print("Taring load cell...");

    delay(2000);

    if (loadCell.is_ready())
    {
        loadCell.tare(20);
        loadCellOK = true;
    }
    else
    {
        loadCellOK = false;
        Serial.println("ERROR: HX711 not detected.");
    }

    // ------------------------------------------------------------------------
    // Initial Pre-Calculations (Constant Fluid Properties)
    // ------------------------------------------------------------------------
    fluidDensity = FLUID_DENSITY;
    kinematicViscosity = KINEMATIC_VISCOSITY;

    if (kinematicViscosity > 0.0)
    {
        reynoldsNumber = (FLOW_VELOCITY * TURBINE_WIDTH) / kinematicViscosity;
    }

    // ------------------------------------------------------------------------
    // Initial System Status
    // ------------------------------------------------------------------------
    bool sensorsOK = temperatureSensorOK && loadCellOK;

    digitalWrite(
        GREEN_STATUS_LED_PIN,
        sensorsOK ? HIGH : LOW
    );

    if (sensorsOK)
    {
        Serial.println("DAQ initialized successfully.");
    }
    else
    {
        Serial.println("DAQ initialized with sensor errors.");
    }

    delay(1000);

    if (temperatureSensorOK)
    {
        float initialTemperature = temperatureSensor.getTempCByIndex(0);

        if (initialTemperature != DEVICE_DISCONNECTED_C &&
            initialTemperature != 85.0)
        {
            temperatureC = initialTemperature;
        }

        temperatureSensor.requestTemperatures();
    }

    lcd.clear();

    // Synchronize timing variables with the start of the experiment.
    unsigned long startTime = millis();

    previousSampleTime = startTime;
    previousTemperatureTime = startTime;
    previousDisplayTime = startTime;
}


// ============================================================================
// MAIN LOOP
// ============================================================================

void loop()
{
    unsigned long currentTime = millis();


    // ========================================================================
    // TEMPERATURE ACQUISITION -- 1 Hz (Monitoring only)
    // ========================================================================

    if (currentTime - previousTemperatureTime >= TEMPERATURE_INTERVAL_MS)
    {
        previousTemperatureTime += TEMPERATURE_INTERVAL_MS;

        if (temperatureSensorOK)
        {
            float newTemperature = temperatureSensor.getTempCByIndex(0);

            if (newTemperature != DEVICE_DISCONNECTED_C &&
                newTemperature != 85.0)
            {
                temperatureC = newTemperature;
                temperatureSensorOK = true;
            }
            else
            {
                temperatureSensorOK = false;
            }

            // Begin the next non-blocking conversion.
            temperatureSensor.requestTemperatures();
        }
    }


    // ========================================================================
    // MAIN DATA ACQUISITION -- 10 Hz
    // ========================================================================

    if (currentTime - previousSampleTime >= SAMPLE_INTERVAL_MS)
    {
        previousSampleTime += SAMPLE_INTERVAL_MS;

        // --------------------------------------------------------------------
        // 1. Rotational Speed
        // --------------------------------------------------------------------
        noInterrupts();
        unsigned long currentPulsePeriod = pulsePeriodMicros;
        unsigned long lastPulseTime = lastPulseTimeMicros;
        interrupts();

        // If no pulse has been detected for 2 s, assume rotor has stopped.
        if (micros() - lastPulseTime > 2000000)
        {
            rotationalSpeedRPM = 0.0;
        }
        else if (currentPulsePeriod > 0)
        {
            float pulseFrequency = 1000000.0 / currentPulsePeriod;

            rotationalSpeedRPM =
                (pulseFrequency * 60.0) /
                PULSES_PER_REVOLUTION;
        }

        angularVelocity =
            rotationalSpeedRPM *
            (2.0 * PI / 60.0);


        // --------------------------------------------------------------------
        // 2. Load-Cell Measurement
        // --------------------------------------------------------------------
        loadCellOK = loadCell.is_ready();

        if (loadCellOK)
        {
            measuredMassGrams = loadCell.get_units(1);
        }

        float measuredMassKg = measuredMassGrams / 1000.0;

        // Suppress very small residual values around the tare condition.
        if (abs(measuredMassKg) < 0.005)
        {
            measuredMassKg = 0.0;
        }

        reactionForce = measuredMassKg * 9.81;
        torque = reactionForce * LEVER_ARM;


        // --------------------------------------------------------------------
        // 3. Turbine Performance Parameters
        // --------------------------------------------------------------------
        mechanicalPower = torque * angularVelocity;

        availablePower =
            0.5 *
            fluidDensity *
            SWEPT_AREA *
            pow(FLOW_VELOCITY, 3);

        if (FLOW_VELOCITY > 0.0)
        {
            tipSpeedRatio =
                (angularVelocity * TURBINE_RADIUS) /
                FLOW_VELOCITY;
        }
        else
        {
            tipSpeedRatio = 0.0;
        }

        if (availablePower > 0.0)
        {
            powerCoefficient = mechanicalPower / availablePower;
        }
        else
        {
            powerCoefficient = 0.0;
        }


        // --------------------------------------------------------------------
        // 4. Sensor Status
        // --------------------------------------------------------------------
        bool sensorsOK = temperatureSensorOK && loadCellOK;

        digitalWrite(
            GREEN_STATUS_LED_PIN,
            sensorsOK ? HIGH : LOW
        );


        // --------------------------------------------------------------------
        // 5. Serial Output -- 10 Hz
        // --------------------------------------------------------------------
        Serial.printf(
            "RPM: %.1f | Force: %.3f N | Torque: %.4f Nm | "
            "Temp: %.2f C | Density: %.1f kg/m3 | Visc: %.3e m2/s | Re: %.0f | "
            "Power: %.3f W | TSR: %.3f | Cp: %.4f\n",
            rotationalSpeedRPM,
            reactionForce,
            torque,
            temperatureC,
            fluidDensity,
            kinematicViscosity,
            reynoldsNumber,
            mechanicalPower,
            tipSpeedRatio,
            powerCoefficient
        );
    }


    // ========================================================================
    // LCD UPDATE -- 2 Hz
    // ========================================================================

    if (currentTime - previousDisplayTime >= DISPLAY_INTERVAL_MS)
    {
        previousDisplayTime += DISPLAY_INTERVAL_MS;

        // Line 1: Rotational speed and temperature
        lcd.setCursor(0, 0);
        lcd.printf(
            "RPM:%-5.1f T:%-4.1f",
            rotationalSpeedRPM,
            temperatureC
        );

        // Line 2: Load-cell reading and reaction force
        lcd.setCursor(0, 1);
        lcd.printf(
            "Mass:%-4.0fg F:%-4.2f",
            measuredMassGrams,
            reactionForce
        );

        // Line 3: Torque and power coefficient
        lcd.setCursor(0, 2);
        lcd.printf(
            "Tq:%-5.3f Cp:%-5.3f",
            torque,
            powerCoefficient
        );

        // Line 4: Tip speed ratio and system status
        lcd.setCursor(0, 3);

        if (temperatureSensorOK && loadCellOK)
        {
            lcd.printf(
                "TSR:%-4.2f SYSTEM OK",
                tipSpeedRatio
            );
        }
        else
        {
            lcd.printf(
                "TSR:%-4.2f SENSOR ERR",
                tipSpeedRatio
            );
        }
    }
}
