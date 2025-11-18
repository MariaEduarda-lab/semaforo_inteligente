# 🚦 Documentação do Projeto: Semáforo Inteligente para Smart City

## Grupo: ChinaTown
* Integrantes: 
&emsp; Bernardo Laurindo: responsável pelo código
&emsp; Felipe Lisak: responsável pelo protótipo
&emsp; Gabriel Mutter: responsável pelo protótipo
&emsp; Maria Eduarda Barbosa: responsável pela conexão com ubidots em código e documentação
&emsp; Rafael Ryu: responsável pelo código
&emsp; Steffane Soares: responsável pela gravação de vídeos e protótipo
&emsp; Yudi Omaki: responsável pelo protótipo e testagem


Esta documentação resume o desenvolvimento de um **Semáforo Inteligente** baseado em ESP32, integrando lógica de tráfego responsiva, detecção de luminosidade (LDR) e conectividade IoT via MQTT (HiveMQ) e Ubidots para monitoramento e controle remoto.

---

### 1. 🏗️ Arquitetura Física e Componentes

O sistema utiliza o microcontrolador ESP32 para gerenciar dois semáforos, um sensor de luminosidade (LDR) e um sensor de proximidade ultrassônico.

| Componente | Função | Pinos (GPIO) |
| :--- | :--- | :--- |
| **Microcontrolador** | ESP32 | - |
| **Semáforo 1 (Principal)** | Controle de fluxo na primeira via. | VM: 18, AM: 19, VD: 5 |
| **Semáforo 2 (Secundário)** | Controle de fluxo na segunda via. | VM: 15, AM: 4, VD: 16 |
| **Sensor de Luminosidade** | LDR (Detecção de Dia/Noite). | 34 |
| **Sensor de Proximidade** | Ultrassônico (Detecção de Veículos). | Trigger: 21, Echo: 22 |



---

### 2. 💡 Lógica de Operação (Parte 1: LDR e Modo Noturno)

O sistema alterna automaticamente entre os modos **Diurno** e **Noturno** com base na leitura do LDR.

#### ☀️ Modo Diurno (Normal e Inteligente)
* **Ativação:** $\text{Valor LDR} > 1200$.
* **Comportamento:** Semáforos seguem um ciclo de tempo pré-definido ($\text{Verde} \rightarrow \text{Amarelo} \rightarrow \text{Vermelho}$).
* **Prioridade Inteligente:** Se o **Sensor Ultrassônico** detectar um veículo ($\le 5\text{cm}$), o semáforo correspondente é forçado **imediatamente** ao estado **VERDE**, garantindo a fluidez do tráfego. O contador de veículos detectados é incrementado.

#### 🌙 Modo Noturno (Pisca-Alerta)
* **Ativação:** $\text{Valor LDR} < 500$.
* **Comportamento:** Ambos os semáforos (Sem 1 e Sem 2) ficam fixos no estado **AMARELO** e piscam a cada $500\text{ms}$.
* **Objetivo:** Reduzir o consumo de energia e manter a atenção dos motoristas em vias de baixo fluxo.

---

### 3. 🌐 Conectividade e Interface Online (Parte 2: IoT)

A comunicação é estabelecida via **Wi-Fi** e gerenciada por dois serviços IoT: HiveMQ para tempo real e Ubidots para a interface.

#### A. Comunicação MQTT (HiveMQ)
Utilizado para comunicação segura (TLS/SSL) e de baixa latência.

* **Tópicos Publicados (Output):**
    * `semaforo/status`: Informa $\text{Modo}$ de operação, $\text{Estado dos Semáforos}$ e $\text{Contador de Veículos}$.
    * `semaforo/sensores`: Envia valores brutos de $\text{LDR}$ e $\text{Distância}$.
* **Tópico Subscrito (Input):**
    * `semaforo/comando`: Permite o controle remoto, como forçar a transição de modos (`modo_diurno`, `modo_noturno`) ou zerar o contador de veículos (`reset`).

#### B. Interface Online (Ubidots Dashboard)
O Ubidots serve como o painel de controle e monitoramento.

* **Monitoramento:**
    * Visualização em tempo real de $\text{LDR}$, $\text{Distância}$ e $\text{Contador de Veículos}$.
    * Exibição do estado atual dos semáforos e modo de operação.
* **Controle:**
    * Permite o envio de comandos para o tópico $\text{MQTT}$ (`semaforo/comando`) via *Widgets* de controle, facilitando a interação e teste.



---

### 4. 📦 Requisitos de Entrega (Barema)
O projeto completo será submetido ao GitHub com os seguintes itens:

1.  **Montagem Física:** Representada por uma foto ou diagrama (documentado em vídeo).
2.  **Código-Fonte:** [ponderada_prog.ino](ponderada_prog.ino)
3.  **Vídeo Demonstrativo:** Comprovação do funcionamento da lógica de $\text{Modo Noturno/LDR}$ e da $\text{Prioridade por Veículo/Ultrassônico}$.
[vídeo1](semaforo1.mp4)
[vídeo2](semaforo2.mp4)
4.  **Como usar o ubidots para ter acesso as variáveis?** O Ubidots atua como um intermediário inteligente (hub de dados) para a Internet das Coisas (IoT). Ele recebe dados brutos (como leituras de $\text{LDR}$ e $\text{distância}$) do seu dispositivo ESP32 via protocolos como MQTT, utilizando um Token da API para autenticação. A plataforma armazena esses dados em Variáveis dentro de um Dispositivo específico, permitindo que você visualize o histórico e o status em tempo real. Além disso, a principal função do Ubidots é permitir a criação de Dashboards (interfaces online) com Widgets (gráficos e indicadores) que transformam esses dados em informações visuais úteis, e, crucialmente, também permite o Controle Remoto, enviando comandos de volta para o ESP32 (como ligar o $\text{Modo Noturno}$) através dos mesmos protocolos de comunicação.
Para ter acesso, é preciso conectar o token **BBUS-hRRfiBCNVYEtObCygTMr0NnD8KCObp** com a label **semaforo_inteligente**. Apesar de tentar ser feito cada detalhe, por problemas com a plataforma, o grupo não conseguiu receber as informações das variáveis, enfrentando dificuldades com tais, apesar de existir toda a conexão e retorno no código.