/*
  Using the MLX90632 FIR Sensor
  By: Nathan Seidle
  SparkFun Electronics
  Date: December 21st, 2017
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/products/14569

  This example shows that the various math and code matches the output
  from the datasheet.

  Hardware Connections:
  You don't need to hook anything up
*/

//#define terminal SerialUSB //Used with SAMD21 Mini Dev
#define terminal Serial //Used with RedBoards

void setup()
{
  terminal.begin(9600);
  while (!terminal); //Wait for terminal to open
  terminal.println("MLX90632 Algorithm Test");

  terminal.print("Size of a double: ");
  terminal.println(sizeof(double));

  //Get constants from datasheet
  long EE_P_R = 0x5D0103;
  long EE_P_G = 0x51CFAE5;
  long EE_P_T = 0x0000000;
  long EE_P_O = 0x1900;
  long EE_Ea = 0x51CFAE;
  long EE_Eb = 0x5D0103;
  long EE_Fa = 0x3506351;
  long EE_Fb = 0xFE2571F1;
  long EE_Ga = 0xFDFFA7A5;

  //8-bit boards pull these in correctly as 16-bit ints
  //ARM brings these in as 32-bit ints.
  //Force 16-bit ints
  int16_t EE_Gb = 0x2600;
  int16_t EE_Ka = 0x2A00;
  int16_t EE_Ha = 0x4000;
  int16_t EE_Hb = 0x0000;

  double P_R = EE_P_R * pow(2, -8);
  double P_G = EE_P_G * pow(2, -20);
  double P_T = EE_P_T * pow(2, -44);
  double P_O = EE_P_O * pow(2, -8);
  double Ea = EE_Ea * pow(2, -16);
  double Eb = EE_Eb * pow(2, -8);
  double Fa = EE_Fa * pow(2, -46);
  double Fb = EE_Fb * pow(2, -36);
  double Ga = EE_Ga * pow(2, -36);
  double Gb = EE_Gb * pow(2, -10);
  double Ka = EE_Ka * pow(2, -10);
  double Ha = EE_Ha * pow(2, -14); //Ha!
  double Hb = EE_Hb * pow(2, -14);

  int16_t RAM_4 = 0xFF9B;
  int16_t RAM_5 = 0xFF9D;
  int16_t RAM_6 = 0x57E4;
  int16_t RAM_7 = 0xFF97;
  int16_t RAM_8 = 0xFF99;
  int16_t RAM_9 = 0x59D8;

  double TOdut = 25.0; //Assume 25C for first iteration
  double TO0 = 25.0; //object temp from last calc
  double TA0 = 25.0; //ambient temp from last calc

  terminal.print("P_R: ");
  terminal.println(P_R);
  terminal.print("P_G: ");
  terminal.println(P_G, 5);
  terminal.print("P_T: ");
  terminal.println(P_T, 5);
  terminal.print("P_O: ");
  terminal.println(P_O, 5);
  terminal.print("Ea: ");
  terminal.println(Ea, 5);
  terminal.print("Eb: ");
  terminal.println(Eb, 5);
  terminal.print("Fa: ");
  terminal.println(Fa, 10);
  terminal.print("Fb: ");
  terminal.println(Fb, 10);
  terminal.print("Ga: ");
  terminal.println(Ga, 5);
  terminal.print("Gb: ");
  terminal.println(Gb, 5);
  terminal.print("Ka: ");
  terminal.println(Ka, 5);
  terminal.print("Ha: ");
  terminal.println(Ha, 5);
  terminal.print("Hb: ");
  terminal.println(Hb, 5);

  //Object temp requires 3 iterations
  for (int i = 0 ; i < 3 ; i++)
  {
    terminal.println();

    double VRta = RAM_9 + Gb * (RAM_6 / 12.0);
    terminal.print("VRta: ");
    terminal.println(VRta);

    double AMB = (RAM_6 / 12.0) / VRta * pow(2, 19);
    terminal.print("AMB: ");
    terminal.println(AMB, 10);

    double sensorTemp = P_O + (AMB - P_R) / P_G + P_T * pow((AMB - P_R), 2);
    terminal.print("sensorTemp (Ta): ");
    terminal.println(sensorTemp, 4);

    float S = (float)(RAM_4 + RAM_5) / 2.0;
    double VRto = RAM_9 + Ka * (RAM_6 / 12);
    double Sto = (S / 12) / VRto * (double)pow(2, 19);
    terminal.print("S: ");
    terminal.println(S);
    terminal.print("VRto: ");
    terminal.println(VRto);
    terminal.print("Sto: ");
    terminal.println(Sto);

    double TAdut = (AMB - Eb) / Ea + 25.0;
    terminal.print("TAdut: ");
    terminal.println(TAdut, 10);

    double ambientTempK = TAdut + 273.15;
    terminal.print("ambientTempK: ");
    terminal.println(ambientTempK, 10);

    double bigFraction = Sto / (1 * Fa * Ha * (1 + Ga * (TOdut - TO0) + Fb * (TAdut - TA0)));
    terminal.print("bigFraction: ");
    terminal.println(bigFraction, 4);

    double objectTemp = bigFraction + pow(ambientTempK, 4);
    objectTemp = pow(objectTemp, 0.25); //Take 4th root
    objectTemp = objectTemp - 273.15 - Hb;

    if (i == 0) terminal.print("Object temp 0 (should be 27.2048027): ");
    if (i == 1) terminal.print("Object temp 1 (should be 27.2035098): ");
    if (i == 2) terminal.print("Object temp 2 (should be 27.2035105): ");
    terminal.println(objectTemp, 7);

    TO0 = objectTemp;
  }
}

void loop()
{

}
