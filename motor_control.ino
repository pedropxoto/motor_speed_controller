#include <LiquidCrystal.h>
#include <Wire.h>
#include <SoftwareSerial.h>

SoftwareSerial PlacaBT(7, 13); // RX | TX

LiquidCrystal lcd(12, 5, 8, 9, 10, 6);

// PARÂMETROS DE CONFIGURAÇÃO
#define FREQ (pow(10, 6) * 16)
#define MAX_TIMER 255
#define PRESCALER_2 0x07
#define PRESCALER_0 0x02
#define PRESCALER_VALUE 8
#define INTERVALO_ESTIMATIVA 1                                                      // estima vel a cada 1s
#define INTERVALO_LIMITE INTERVALO_ESTIMATIVA *FREQ / (PRESCALER_VALUE * pow(2, 8)) // valor que o counter precisa atingir para dar intervalo
#define VELOCIDADE_MAXIMA 4688
#define FPS 50

// PARAMETROS DE PINOUT
#define P1A 2
#define P2A 4
#define EN 11
#define SAIDA_INVERSOR 3

// VARIÁVELS GLOBAIS

// comando
char comando[15];
char interpretando_comando = false;
// ESTADO PARA ESTIMATIVA DA VELOCIDADE

// CONTADORES
unsigned int counter = 0;
unsigned int counter_estimativa = 0;
unsigned int counter_display = 0;

// velocidade atual
float velocidade = 0;

// display atual
char display_ativado = 0;

// flags
char exibe_lcd = false;
char parado = false;
char mostra_display = false;
char girar_motor = false;

void setup()
{
  // CONFIGURAÇÃO DE PINOS
  cli();

  pinMode(P1A, OUTPUT);                  // entrada 01
  pinMode(P2A, OUTPUT);                  // entrada 02
  pinMode(EN, OUTPUT);                   // enable ponte H
  pinMode(SAIDA_INVERSOR, INPUT_PULLUP); // saída do inversor

  // CONFIGURAÇÃO DO TEMPORIZADOR
  attachInterrupt(digitalPinToInterrupt(SAIDA_INVERSOR), inversor, FALLING);
  ajuste_PWM(100);

  Wire.begin();

  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("ROTACAO:");
  lcd.setCursor(13, 0);
  lcd.print("RPM");
  lcd.setCursor(3, 1);
  lcd.print("ESTIMATIVA");
  lcd.setCursor(8, 0);

  // CONFIGURAÇÃO UART
  Serial.begin(9600);
  PlacaBT.begin(9600);

  // configuracao_Timer2();
  configuracao_Timer0();
  configuracao_Timer2();

  instrucoes_motor("EXAU");

  sei();
}

void loop()
{
  le_comando();
  if (exibe_lcd)
  {
    get_velocidade();

    exibe_lcd = false;
  }
  else if (parado)
  {
    lcd.setCursor(8, 0);
    lcd.print("    ");
    lcd.setCursor(8, 0);
    lcd.print("0");

    if (girar_motor == 1)
    {
      digitalWrite(P1A, 0);
      digitalWrite(P2A, 1);
      girar_motor = false;
    }

    if (girar_motor == 2)
    {
      digitalWrite(P1A, 1);
      digitalWrite(P2A, 0);
      girar_motor = false;
    }

    parado = false;
  }
  if (mostra_display)
  {
    envia_velocidade();
    mostra_display = false;
  }
}

ISR(TIMER0_COMPA_vect)
{
  counter_estimativa++;
  // verifica se não excedeu INTERVALO_LIMITE sem mudar de estado

  if (counter_display >= 40)
  {
    mostra_display = true;
    counter_display = 0;
  }
  if (!interpretando_comando && (counter > INTERVALO_LIMITE && counter_estimativa > INTERVALO_LIMITE))
  {
    parado = true;
    counter = 0;
    velocidade = 0;
    counter_estimativa = 0;
  }
  counter++;
  counter_display++;
}

void inversor()
{
  if (counter_estimativa > INTERVALO_LIMITE)
  {
    exibe_lcd = true;
    velocidade = (60 / (counter * 2 * PRESCALER_VALUE * pow(2, 8) / FREQ));
    counter_estimativa = 0;
  }
  counter = 0;
}

void instrucoes_motor(char *instrucao)
{
  // executa uma das quatro operações do motor
  char cmp_instrucoes[4][7] = {"VENT", "EXAU", "PARA", "FPARA"};
  if (strcmp(instrucao, cmp_instrucoes[0]) == 0)
  {
    ajuste_PWM(100);
    digitalWrite(P1A, 0);
    digitalWrite(P2A, 0);
    girar_motor = 1;

    return;
  }

  if (strcmp(instrucao, cmp_instrucoes[1]) == 0)
  {
    ajuste_PWM(100);
    digitalWrite(P1A, 0);
    digitalWrite(P2A, 0);
    girar_motor = 2;

    return;
  }

  if (strcmp(instrucao, cmp_instrucoes[2]) == 0)
  {
    // para naturalmente
    ajuste_PWM(0);
    return;
  }

  if (strcmp(instrucao, cmp_instrucoes[3]) == 0)
  {
    // força parada
    digitalWrite(P1A, 0);
    digitalWrite(P2A, 0);
    return;
  }
  return;
}

void ajusta_velocidade(float vel)
{
  float duty_cicle = 100 * pow((float)vel / VELOCIDADE_MAXIMA, 2);

  if (duty_cicle <= 100)
    ajuste_PWM(duty_cicle);

  return;
}

void ajuste_PWM(float percent)
{
  // regula valor de 0CR2A de acordo com o percentual almeijado
  if (percent > 100)
    OCR2A = (int)(100 * (MAX_TIMER) / 100);
  if (percent < 0)
    OCR2A = (int)(0 * (MAX_TIMER) / 100);

  OCR2A = (int)(percent * (MAX_TIMER) / 100);

  return;
}

void get_velocidade()
{
  lcd.setCursor(8, 0);
  lcd.print("    ");
  lcd.setCursor(8, 0);
  lcd.print((int)velocidade);
  lcd.setCursor(8, 0);
  return;
}

unsigned int get_periodo()
{
  return PRESCALER_VALUE * pow(2, 8) / FREQ;
}

void le_comando()
{
  interpretando_comando = true;
  char comando_temp[15];
  char i = 0;

  if (Serial.available() > 0)
  {

    // lê comando enviado
    String comando_enviado = Serial.readString();

    // itera para filtrar somente o comando
    while (i < 15)
    {
      if (comando_enviado[i] == '*')
      {
        // verifica término do comando
        comando_temp[i] = '\0';
        envia_resposta(comando_temp);
        return;
      }
      comando_temp[i] = comando_enviado[i];
      i++;
    }
    Serial.println("ERRO: COMANDO INEXISTENTE");
  }

  if (PlacaBT.available() > 0)
  {

    // lê comando enviado
    String comando_enviado = PlacaBT.readString();

    // itera para filtrar somente o comando
    while (i < 15)
    {
      if (comando_enviado[i] == '*')
      {
        // verifica término do comando
        comando_temp[i] = '\0';
        envia_resposta_bt(comando_temp);
        return;
      }
      comando_temp[i] = comando_enviado[i];
      i++;
    }
    PlacaBT.println("ERRO: COMANDO INEXISTENTE");
  }

  interpretando_comando = false;
  return;
}

void envia_resposta_bt(char *comando_temp)
{
  // verifica qual é o comando enviado e envia devida resposta
  char cmp_instrucoes[7][7] = {"VENT", "EXAU", "PARA", "FPARA", "RETV", "VEL", "VREL"};

  if (strcmp(comando_temp, cmp_instrucoes[0]) == 0)
  {
    PlacaBT.println("OK: VENTILAÇÃO");
    instrucoes_motor(comando_temp);
    return;
  }

  if (strcmp(comando_temp, cmp_instrucoes[1]) == 0)
  {
    PlacaBT.println("OK: EXAUSTÃO");
    instrucoes_motor(comando_temp);

    return;
  }

  if (strcmp(comando_temp, cmp_instrucoes[2]) == 0)
  {
    PlacaBT.println("OK: PARAR");
    instrucoes_motor(comando_temp);
    return;
  }

  if (strcmp(comando_temp, cmp_instrucoes[3]) == 0)
  {
    PlacaBT.println("OK: FORÇAR PARAR");
    instrucoes_motor(comando_temp);

    return;
  }

  if (strcmp(comando_temp, cmp_instrucoes[4]) == 0)
  {
    PlacaBT.println("OK: VEL = x RPM");

    return;
  }

  if (strcmp(String(comando_temp).substring(0, 3).c_str(), cmp_instrucoes[5]) == 0)
  {
    String comando_str(comando_temp);

    comando_str.remove(0, 3);
    if (!comando_str.length())
    {
      PlacaBT.println("ERRO: PARÂMETRO AUSENTE");
      return;
    }
    char vel_parameter[comando_str.length() + 1];
    comando_str.toCharArray(vel_parameter, sizeof(vel_parameter));

    int parameter_int = string_2_int(vel_parameter);
    if (parameter_int == -1 || parameter_int < 0 || parameter_int > VELOCIDADE_MAXIMA)
    {
      PlacaBT.println("ERRO: PARÂMETRO INCORRETO");
      return;
    }

    ajusta_velocidade(parameter_int);
    PlacaBT.print("OK: VEL = x => ");
    PlacaBT.print(vel_parameter);
    PlacaBT.println(" RPM");

    return;
  }

  if (strcmp(String(comando_temp).substring(0, 4).c_str(), cmp_instrucoes[6]) == 0)
  {

    String comando_str(comando_temp);

    if (comando_temp[4] != '-' && comando_temp[4] != '+')
    {
      comando_str.remove(0, 4);
    }
    else
    {
      comando_str.remove(0, 5);
    }

    if (!comando_str.length())
    {
      PlacaBT.println("ERRO: PARÂMETRO AUSENTE");
      return;
    }
    char vel_parameter[comando_str.length() + 1];
    comando_str.toCharArray(vel_parameter, sizeof(vel_parameter));

    int parametro = string_2_int(vel_parameter);
    float nova_velocidade = comando_temp[4] == '-' ? (1 - (float)parametro / 100) * velocidade : (1 + (float)parametro / 100) * velocidade;
    if (parametro == -1 || nova_velocidade > VELOCIDADE_MAXIMA || nova_velocidade < 0)
    {
      PlacaBT.println("ERRO: PARÂMETRO INCORRETO");
      return;
    }
    ajusta_velocidade(nova_velocidade);
    PlacaBT.print("OK: VEL = x => ");
    if (comando_temp[4] == '-' || comando_temp[4] == '+')
    {
      PlacaBT.print(comando_temp[4]);
    }
    PlacaBT.print(vel_parameter);
    PlacaBT.println("%");

    return;
  }

  PlacaBT.println("ERRO: COMANDO INEXISTENTE");

  return;
}

void envia_resposta(char *comando_temp)
{
  // verifica qual é o comando enviado e envia devida resposta
  char cmp_instrucoes[7][7] = {"VENT", "EXAU", "PARA", "FPARA", "RETV", "VEL", "VREL"};

  if (strcmp(comando_temp, cmp_instrucoes[0]) == 0)
  {
    Serial.println("OK: VENTILAÇÃO");
    instrucoes_motor(comando_temp);
    return;
  }

  if (strcmp(comando_temp, cmp_instrucoes[1]) == 0)
  {
    Serial.println("OK: EXAUSTÃO");
    instrucoes_motor(comando_temp);

    return;
  }

  if (strcmp(comando_temp, cmp_instrucoes[2]) == 0)
  {
    Serial.println("OK: PARAR");
    instrucoes_motor(comando_temp);
    return;
  }

  if (strcmp(comando_temp, cmp_instrucoes[3]) == 0)
  {
    Serial.println("OK: FORÇAR PARAR");
    instrucoes_motor(comando_temp);

    return;
  }

  if (strcmp(comando_temp, cmp_instrucoes[4]) == 0)
  {
    Serial.println("OK: VEL = x RPM");

    return;
  }

  if (strcmp(String(comando_temp).substring(0, 3).c_str(), cmp_instrucoes[5]) == 0)
  {
    String comando_str(comando_temp);

    comando_str.remove(0, 3);
    if (!comando_str.length())
    {
      Serial.println("ERRO: PARÂMETRO AUSENTE");
      return;
    }
    char vel_parameter[comando_str.length() + 1];
    comando_str.toCharArray(vel_parameter, sizeof(vel_parameter));

    int parameter_int = string_2_int(vel_parameter);
    if (parameter_int == -1 || parameter_int < 0 || parameter_int > VELOCIDADE_MAXIMA)
    {
      Serial.println("ERRO: PARÂMETRO INCORRETO");
      return;
    }

    ajusta_velocidade(parameter_int);
    Serial.print("OK: VEL = x => ");
    Serial.print(vel_parameter);
    Serial.println(" RPM");

    return;
  }

  if (strcmp(String(comando_temp).substring(0, 4).c_str(), cmp_instrucoes[6]) == 0)
  {

    String comando_str(comando_temp);

    if (comando_temp[4] != '-' && comando_temp[4] != '+')
    {
      comando_str.remove(0, 4);
    }
    else
    {
      comando_str.remove(0, 5);
    }

    if (!comando_str.length())
    {
      Serial.println("ERRO: PARÂMETRO AUSENTE");
      return;
    }
    char vel_parameter[comando_str.length() + 1];
    comando_str.toCharArray(vel_parameter, sizeof(vel_parameter));

    int parametro = string_2_int(vel_parameter);
    float nova_velocidade = comando_temp[4] == '-' ? (1 - (float)parametro / 100) * velocidade : (1 + (float)parametro / 100) * velocidade;
    if (parametro == -1 || nova_velocidade > VELOCIDADE_MAXIMA || nova_velocidade < 0)
    {
      Serial.println("ERRO: PARÂMETRO INCORRETO");
      return;
    }
    ajusta_velocidade(nova_velocidade);
    Serial.print("OK: VEL = x => ");
    if (comando_temp[4] == '-' || comando_temp[4] == '+')
    {
      Serial.print(comando_temp[4]);
    }
    Serial.print(vel_parameter);
    Serial.println("%");

    return;
  }

  Serial.println("ERRO: COMANDO INEXISTENTE");

  return;
}

int string_2_int(char *string)
{
  // transformação de string para inteiro
  int soma = 0;

  int j = 0;
  for (int i = (strlen(string) - 1); i >= 0; i--)
  {

    if (string[i] > 57 || string[i] < 48)
    {
      // parâmetro inválido
      return -1;
    }
    // string para decimal
    soma = round(soma + (string[j] - 48) * pow(10, i));

    j++;
  }

  return soma;
}

void configuracao_Timer2()
{
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Configuracao Temporizador 2 (8 bits) para gerar interrupcoes periodicas a cada 8ms no modo Clear Timer on Compare Match (CTC)
  // Relogio = 16e6 Hz
  // Prescaler = 1024
  // Faixa = 125 (contagem de 0 a OCR2A = 124)
  // Intervalo entre interrupcoes: (Prescaler/Relogio)*Faixa = (64/16e6)*(124+1) = 0.008s

  // TCCR0A – Timer/Counter Control Register A
  // COM2A1 COM2A0 COM2B1 COM2B0 – – WGM21 WGM20
  // 1      0      0      0          1     1
  TCCR2A = 0x83;

  // OCR2A – Output Compare Register A

  // TIMSK0 – Timer/Counter Interrupt Mask Register
  // – – – – – OCIE2B OCIE2A TOIE2
  // – – – – – 0      1      0
  TIMSK2 = 0x00;

  // TCCR2B – Timer/Counter Control Register B
  // FOC2A FOC2B – – WGM22 CS22 CS21 CS2
  // 0     0         0     0    1    0
  TCCR2B = PRESCALER_2;
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////
}

void envia_velocidade()
{
  float vel = round(velocidade);

  // encontra digito a ser enviado
  char i = 0;
  int digito_vel = (int)(vel) % 10;
  while (i < display_ativado)
  {
    vel = floor(vel / 10);
    digito_vel = (int)(vel) % 10;

    i++;
  }

  // verifica se deve ativar ou não o display
  if (digito_vel == 0 && velocidade < pow(10, display_ativado))
  {
    escreve_8574(5, digito_vel);
  }
  else
  {
    escreve_8574(3 - display_ativado, digito_vel);
  }

  if (display_ativado == 3)
  {
    display_ativado = 0;
  }
  else
  {
    display_ativado++;
  }

  return;
}

void escreve_8574(unsigned char display, unsigned char value)
{
  char data = (~(1 << display) << 4) | value;
  // inicia transmissão
  Wire.beginTransmission(0x20);
  // realizar operação de escrita no chip 8574
  Wire.write(data);
  // termina operação de transmissão
  Wire.endTransmission();
}

void configuracao_Timer0()
{
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Configuracao Temporizador 2 (8 bits) para gerar interrupcoes periodicas a cada 8ms no modo Clear Timer on Compare Match (CTC)
  // Relogio = 16e6 Hz
  // Prescaler = 1024
  // Faixa = 125 (contagem de 0 a OCR2A = 124)
  // Intervalo entre interrupcoes: (Prescaler/Relogio)*Faixa = (64/16e6)*(124+1) = 0.008s

  // TCCR0A – Timer/Counter Control Register A
  // COM2A1 COM2A0 COM2B1 COM2B0 – – WGM21 WGM20
  // 1      0      0      0          1     1
  TCCR0A = 0x00;

  // OCR2A – Output Compare Register A

  // TIMSK0 – Timer/Counter Interrupt Mask Register
  // – – – – – OCIE2B OCIE2A TOIE2
  // – – – – – 0      1      0
  TIMSK0 |= (1 << OCIE0A);
  OCR0A = 255;
  // TCCR2B – Timer/Counter Control Register B
  // FOC2A FOC2B – – WGM22 CS22 CS21 CS2
  // 0     0         0     0    1    0
  TCCR0B = PRESCALER_0;
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////
}
