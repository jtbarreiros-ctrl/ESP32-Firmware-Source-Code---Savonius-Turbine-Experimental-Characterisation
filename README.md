# Savonius Turbine - Data Acquisition System (DAQ)



Real-time data acquisition and monitoring system developed for the experimental characterization of a vertical-axis Savonius turbine. This repository integrates the ESP32 firmware (sensor reading and dimensionless performance calculation at 10 Hz) and the Python interface script for live plotting and automatic CSV logging.



## Repository Structure



* Savonius_turbine_exp_characterisation.ino: C++/Arduino firmware for the ESP32 microcontroller. Acquires rotational speed, reaction force from a Prony brake dynamometer, and fluid temperature at 10 Hz, displays telemetry locally on a 20x4 I2C LCD, and streams structured data via Serial.

* Data_capture_excel.py: PC-side acquisition and visualization script. Parses the incoming serial stream, generates two real-time plots (RPM & Torque vs. Time and Cp vs. TSR), and automatically saves 1200-sample test runs (120 s) to CSV files.



## Hardware & Pinout (ESP32)



The firmware supports two experimental modes - Open-Water (OPEN_WATER_TEST) and Wind Tunnel (WIND_TUNNEL_TEST) - automatically selecting the corresponding load-cell calibration factor and fluid properties.



* Hall-Effect Sensor: GPIO 5 -> Rotational speed via external interrupt (2 magnets/rev, 10 ms debounce)

* Load Cell + HX711: GPIO 16 (DT) / GPIO 17 (SCK) -> Reaction force measurement on the Prony brake lever arm (L = 0.06 m)

* DS18B20 Sensor: GPIO 4 -> Fluid temperature monitoring (OneWire protocol, non-blocking at 1 Hz)

* 20x4 LCD Display (I2C): SDA / SCL (0x27) -> Local display (2 Hz refresh) showing RPM, Temp, Mass, Force, Torque, Cp, and TSR

* Status LED (Green): GPIO 19 -> Sensor health indicator (HIGH = System OK)



## Computed Parameters



Based on the rotor geometry (H = 0.230 m, D = 0.244 m, A = 0.05612 m^2) and the defined flow velocity (U), the system computes in real time:



* Angular Velocity (omega) and Mechanical Torque (T = F ⋅ L)

* Mechanical Power (P_mech = T ⋅ omega) and Available Fluid Power (P_a = 0.5 ⋅ rho ⋅ A ⋅ U^3)

* Tip Speed Ratio (TSR = (omega ⋅ R) / U)

* Power Coefficient (Cp = P_mech / P_a) and Reynolds Number (Re = (U ⋅ D) / nu)



## Prerequisites & Installation



### 1. ESP32 Firmware (Arduino IDE / PlatformIO)

Install the following libraries before compiling and uploading to the ESP32:

* LiquidCrystal_I2C

* OneWire & DallasTemperature

* HX711



### 2. Python Environment

Install the required packages for serial communication and live plotting:



pip install pyserial matplotlib



## Usage Instructions



1. Configure and Flash the ESP32:

   * In Savonius_turbine_exp_characterisation.ino, select the testing medium by uncommenting #define OPEN_WATER_TEST or #define WIND_TUNNEL_TEST.

   * Set the target flow velocity in FLOW_VELOCITY (e.g., 1.0, 1.5, or 2.0 m/s for water; 8.0, 10.0, or 12.0 m/s for air) and upload the sketch.

   * Upon power-up, keep the Prony brake unloaded while the load cell performs its automatic initial tare.



2. Run the Python Logger:

   * Open Data_capture_excel.py and verify the serial port (PORTA = "COM6") and flow velocity (VELOCIDADE_ESCOAMENTO = 1.0).

   * Run the script:

     python Data_capture_excel.py

   * The real-time monitoring window will open. Acquisition automatically stops after 1200 valid data points (~120 seconds at 10 Hz) or when the plot window is manually closed, saving the dataset as dados_savonius_agua_<velocity>ms_<timestamp>.csv.



## CSV Output Format



Generated files use ';' as a delimiter and contain the following columns:



Ponto; Data_Hora; Tempo_Ensaio_s; RPM; Forca_N; Torque_Nm; Temperatura_C; Densidade_kg_m3; Viscosidade_m2_s; Reynolds; Velocidade_Escoamento_ms; Potencia_Mecanica_W; TSR; Cp
