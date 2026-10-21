# -*- coding: utf-8 -*-
"""
Created on Fri Oct 16 00:56:37 2026

@author: jtbar
"""

import serial
import csv
import time
from datetime import datetime
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import sys


# =======================================================
# CONFIGURAÇÕES
# =======================================================

PORTA = "COM6"
BAUD_RATE = 115200

NUM_PONTOS = 1200

# Velocidade definida para este ensaio em água (1.0, 1.5 ou 2.0 m/s)
VELOCIDADE_ESCOAMENTO = 1.0  # [m/s]


# =======================================================
# LIMITES FIXOS DOS GRÁFICOS (Ajustados para Água)
# =======================================================

# Eixo vertical das RPM
RPM_Y_MIN = 0
RPM_Y_MAX = 200

# Eixo vertical do Torque [Nm]
TORQUE_Y_MIN = 0
TORQUE_Y_MAX = 0.2

# Eixo vertical do Cp
CP_Y_MIN = 0
CP_Y_MAX = 0.1


# =======================================================
# NOME DO FICHEIRO
# =======================================================

timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

nome_ficheiro = (
    f"dados_savonius_agua_"
    f"{VELOCIDADE_ESCOAMENTO:.1f}ms_"
    f"{timestamp}.csv"
)


# =======================================================
# VARIÁVEIS DOS GRÁFICOS
# =======================================================

tempos = []
rpms = []
torques = []
cps = []
tsrs = []


# =======================================================
# CONTROLO DO ENSAIO
# =======================================================

contador_pontos = 0
tempo_inicio = None

ensaio_terminado = False
janela_aberta = True


# =======================================================
# INICIALIZAÇÃO DA PORTA SÉRIE
# =======================================================

try:

    ser = serial.Serial()

    ser.port = PORTA
    ser.baudrate = BAUD_RATE
    ser.timeout = 0.1

    # Tenta evitar reset automático do ESP32
    # ao abrir a porta série.
    ser.dtr = False
    ser.rts = False

    ser.open()

    print(f"[SUCESSO] Conectado à {PORTA}.")

except Exception as e:

    print(
        f"[ERRO] Não foi possível abrir "
        f"a porta {PORTA}."
    )

    print(f"Detalhes: {e}")

    sys.exit(1)


# =======================================================
# CRIAÇÃO DO CSV
# =======================================================

ficheiro_csv = open(
    nome_ficheiro,
    mode="w",
    newline="",
    encoding="utf-8"
)

escritor_csv = csv.writer(
    ficheiro_csv,
    delimiter=";"
)


# Cabeçalho do ficheiro
escritor_csv.writerow([
    "Ponto",
    "Data_Hora",
    "Tempo_Ensaio_s",
    "RPM",
    "Forca_N",
    "Torque_Nm",
    "Temperatura_C",
    "Densidade_kg_m3",
    "Viscosidade_m2_s",
    "Reynolds",
    "Velocidade_Escoamento_ms",
    "Potencia_Mecanica_W",
    "TSR",
    "Cp"
])


# =======================================================
# CONFIGURAÇÃO DOS GRÁFICOS
# =======================================================

plt.style.use("fast")

fig = plt.figure(
    figsize=(10, 8)
)

fig.canvas.manager.set_window_title(
    "Monitorização Savonius em Tempo Real - Ensaios em Água"
)


# -------------------------------------------------------
# Gráfico superior
# RPM + Torque
# -------------------------------------------------------

ax1 = fig.add_subplot(
    2,
    1,
    1
)

ax1_torque = ax1.twinx()


# -------------------------------------------------------
# Gráfico inferior
# Cp vs TSR
# -------------------------------------------------------

ax2 = fig.add_subplot(
    2,
    1,
    2
)


fig.tight_layout(
    pad=4.0
)


# =======================================================
# FUNÇÃO PARA FECHAR CONEXÕES
# =======================================================

def fechar_conexoes():

    global janela_aberta

    janela_aberta = False

    try:

        if ser.is_open:
            ser.close()

    except Exception:
        pass

    try:

        if not ficheiro_csv.closed:

            ficheiro_csv.flush()
            ficheiro_csv.close()

    except Exception:
        pass


# =======================================================
# FECHO MANUAL DA JANELA
# =======================================================

def fechar_janela(event):

    if not ensaio_terminado:

        print()

        print(
            "[PARAGEM MANUAL] "
            "Ensaio interrompido."
        )

        print(
            f"[INFO] Foram adquiridos "
            f"{contador_pontos}/{NUM_PONTOS} pontos."
        )

    fechar_conexoes()

    print(
        f"[SUCESSO] Dados guardados em:"
    )

    print(
        nome_ficheiro
    )


fig.canvas.mpl_connect(
    "close_event",
    fechar_janela
)


# =======================================================
# AQUISIÇÃO E ATUALIZAÇÃO DOS GRÁFICOS
# =======================================================

def atualizar_grafico(frame):

    global contador_pontos
    global tempo_inicio
    global ensaio_terminado

    global tempos
    global rpms
    global torques
    global cps
    global tsrs


    if not janela_aberta:
        return


    if not ser.is_open:
        return


    if ensaio_terminado:
        return


    try:

        # ===================================================
        # LER TODAS AS LINHAS DISPONÍVEIS
        # ===================================================

        while ser.in_waiting > 0:

            linha = (
                ser.readline()
                .decode(
                    "utf-8",
                    errors="ignore"
                )
                .strip()
            )


            if not linha:
                continue


            print(
                f"[ESP32] {linha}"
            )


            # ===================================================
            # VERIFICAR SE É UMA LINHA DE DADOS
            # ===================================================

            if (
                "RPM:" not in linha
                or "Force:" not in linha
                or "Torque:" not in linha
                or "Cp:" not in linha
            ):
                continue


            try:

                partes = linha.split("|")


                # -----------------------------------------------
                # 0. RPM
                # -----------------------------------------------
                rpm = float(
                    partes[0]
                    .replace("RPM:", "")
                    .strip()
                )

                # -----------------------------------------------
                # 1. FORÇA
                # -----------------------------------------------
                forca = float(
                    partes[1]
                    .replace("Force:", "")
                    .replace("N", "")
                    .strip()
                )

                # -----------------------------------------------
                # 2. TORQUE
                # -----------------------------------------------
                torque = float(
                    partes[2]
                    .replace("Torque:", "")
                    .replace("Nm", "")
                    .strip()
                )

                # -----------------------------------------------
                # 3. TEMPERATURA
                # -----------------------------------------------
                temperatura = float(
                    partes[3]
                    .replace("Temp:", "")
                    .replace("C", "")
                    .strip()
                )

                # -----------------------------------------------
                # 4. DENSIDADE
                # -----------------------------------------------
                densidade = float(
                    partes[4]
                    .replace("Density:", "")
                    .replace("kg/m3", "")
                    .strip()
                )

                # -----------------------------------------------
                # 5. VISCOSIDADE CINEMÁTICA
                # -----------------------------------------------
                viscosidade = float(
                    partes[5]
                    .replace("Visc:", "")
                    .replace("m2/s", "")
                    .strip()
                )

                # -----------------------------------------------
                # 6. NÚMERO DE REYNOLDS
                # -----------------------------------------------
                reynolds = float(
                    partes[6]
                    .replace("Re:", "")
                    .strip()
                )

                # -----------------------------------------------
                # 7. POTÊNCIA MECÂNICA
                # -----------------------------------------------
                potencia = float(
                    partes[7]
                    .replace("Power:", "")
                    .replace("W", "")
                    .strip()
                )

                # -----------------------------------------------
                # 8. TSR
                # -----------------------------------------------
                tsr = float(
                    partes[8]
                    .replace("TSR:", "")
                    .strip()
                )

                # -----------------------------------------------
                # 9. Cp
                # -----------------------------------------------
                cp = float(
                    partes[9]
                    .replace("Cp:", "")
                    .strip()
                )


            except (
                ValueError,
                IndexError
            ) as e:

                print(
                    "[AVISO] Linha inválida "
                    f"ignorada: {e}"
                )

                continue


            # ===================================================
            # PRIMEIRO PONTO DO ENSAIO
            # ===================================================

            if tempo_inicio is None:

                tempo_inicio = (
                    time.perf_counter()
                )

                print()
                print("======================================")
                print("[INÍCIO DA AQUISIÇÃO]")
                print("Primeiro ponto válido recebido.")
                print("======================================")
                print()


            # ===================================================
            # TEMPO DECORRIDO E REGISTO
            # ===================================================

            tempo_decorrido = (
                time.perf_counter()
                - tempo_inicio
            )

            contador_pontos += 1

            agora = datetime.now().strftime(
                "%Y-%m-%d %H:%M:%S.%f"
            )[:-3]


            # ===================================================
            # GUARDAR NAS LISTAS
            # ===================================================

            tempos.append(tempo_decorrido)
            rpms.append(rpm)
            torques.append(torque)
            tsrs.append(tsr)
            cps.append(cp)


            # ===================================================
            # GUARDAR NO CSV
            # ===================================================

            escritor_csv.writerow([
                contador_pontos,
                agora,
                round(tempo_decorrido, 3),
                rpm,
                forca,
                torque,
                temperatura,
                densidade,
                viscosidade,
                reynolds,
                VELOCIDADE_ESCOAMENTO,
                potencia,
                tsr,
                cp
            ])

            # Força imediatamente a escrita no disco
            ficheiro_csv.flush()


            # ===================================================
            # INFORMAÇÃO NO TERMINAL
            # ===================================================

            print(
                f"[PONTO] "
                f"{contador_pontos}/{NUM_PONTOS}"
            )


            # ===================================================
            # TERMINAR AOS 1200 PONTOS
            # ===================================================

            if contador_pontos >= NUM_PONTOS:

                ensaio_terminado = True
                duracao = tempo_decorrido

                print()
                print("======================================")
                print("        ENSAIO CONCLUÍDO")
                print("======================================")
                print(f"Pontos adquiridos: {contador_pontos}")
                print(f"Duração: {duracao:.2f} s")

                if duracao > 0:
                    frequencia_media = (contador_pontos - 1) / duracao
                    print(f"Frequência média: {frequencia_media:.2f} Hz")

                print(f"Ficheiro: {nome_ficheiro}")
                print("======================================")
                print()

                ficheiro_csv.flush()

                fig.suptitle(
                    f"Ensaio concluído - {NUM_PONTOS} pontos",
                    fontsize=14
                )

                atualizar_plots()
                return

    except serial.SerialException as e:
        print(f"[ERRO SÉRIE] {e}")

    except Exception as e:
        print(f"[AVISO] Falha ao processar dados: {e}")

    atualizar_plots()


# =======================================================
# FUNÇÃO DOS GRÁFICOS
# =======================================================

def atualizar_plots():

    if len(tempos) == 0:
        return

    # ---------------------------------------------------
    # GRÁFICO 1: RPM + TORQUE AO LONGO DO TEMPO
    # ---------------------------------------------------
    ax1.clear()
    ax1_torque.clear()

    ax1.set_title(
        "Estabilidade do Ensaio "
        f"({contador_pontos}/{NUM_PONTOS} pontos)"
    )

    ax1.set_xlabel("Tempo (s)")
    ax1.set_ylabel("Rotações (RPM)", color="blue")

    ax1.plot(tempos, rpms, color="blue", label="RPM")
    ax1.tick_params(axis="y", labelcolor="blue")
    ax1.set_ylim(RPM_Y_MIN, RPM_Y_MAX)
    ax1.grid(True, linestyle="--", alpha=0.6)

    ax1_torque.set_ylabel("Torque (Nm)", color="red")
    ax1_torque.plot(tempos, torques, color="red", label="Torque")
    ax1_torque.tick_params(axis="y", labelcolor="red")
    ax1_torque.set_ylim(TORQUE_Y_MIN, TORQUE_Y_MAX)

    # ---------------------------------------------------
    # GRÁFICO 2: Cp VS TSR
    # ---------------------------------------------------
    ax2.clear()

    ax2.set_title("Curva de Desempenho (Cp vs TSR)")
    ax2.set_xlabel("Tip Speed Ratio (TSR)")
    ax2.set_ylabel("Coeficiente de Potência (Cp)")

    ax2.scatter(
        tsrs,
        cps,
        color="green",
        marker="o",
        alpha=0.7
    )

    ax2.grid(True, linestyle="--", alpha=0.6)
    ax2.set_ylim(CP_Y_MIN, CP_Y_MAX)

    fig.canvas.draw_idle()


# =======================================================
# ANIMAÇÃO
# =======================================================

ani = animation.FuncAnimation(
    fig,
    atualizar_grafico,
    interval=250,
    cache_frame_data=False
)


# =======================================================
# INFORMAÇÃO INICIAL
# =======================================================

print()
print("======================================")
print("      SAVONIUS - ENSAIOS EM ÁGUA")
print("======================================")
print(f"Velocidade da água: {VELOCIDADE_ESCOAMENTO:.1f} m/s")
print(f"Número de pontos: {NUM_PONTOS}")
print("Frequência esperada: 10 Hz")
print("Duração esperada: aproximadamente 120 s")
print("[INFO] O Python NÃO efetua tare da célula de carga.")
print("[INFO] Aguarde até RPM e torque estabilizarem antes de iniciar.")
print("[INFO] Fecha a janela para interromper manualmente.")
print("======================================")
print()

plt.show(block=True)

fechar_conexoes()

print()
print("[SUCESSO] Ficheiro guardado em:")
print(nome_ficheiro)