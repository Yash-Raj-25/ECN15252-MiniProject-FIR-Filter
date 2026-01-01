#include <LPC213x.H>
#include <math.h>
#include <string.h>

#define K 64
#define N 512
#define M_PI 3.14159265358979323846

/* ---------------- FIR COEFFICIENTS ---------------- */

/* LOW PASS */
float h_low[K] = {
    -0.000757,  -0.000497,  -0.000000,   0.000644,   0.001245,   0.001507,   0.001134,  -0.000000, 
    -0.001675,  -0.003269,  -0.003917,  -0.002882,  -0.000000,   0.004006,   0.007579,   0.008820, 
     0.006320,   0.000000,  -0.008423,  -0.015697,  -0.018079,  -0.012884,  -0.000000,   0.017307, 
     0.032770,   0.038763,   0.028789,   0.000000,  -0.045174,  -0.098993,  -0.150151,  -0.186837, 
     0.800659,  -0.186837,  -0.150151,  -0.098993,  -0.045174,   0.000000,   0.028789,   0.038763, 
     0.032770,   0.017307,  -0.000000,  -0.012884,  -0.018079,  -0.015697,  -0.008423,   0.000000, 
     0.006320,   0.008820,   0.007579,   0.004006,  -0.000000,  -0.002882,  -0.003917,  -0.003269, 
    -0.001675,  -0.000000,   0.001134,   0.001507,   0.001245,   0.000644,  -0.000000,  -0.000497, 
};

/* HIGH PASS */
float h_high[K] = {
			0.002044, 0.007806, 0.014554, 0.020018, 0.024374, 0.027780, 0.030370, 0.032264,
			0.033568, 0.034372, 0.034757, 0.034791, 0.034534, 0.034040, 0.033353, 0.032511,
			0.031549, 0.030496, 0.029375, 0.028207, 0.027010, 0.025800, 0.024587, 0.023383,
			0.022195, 0.021031, 0.019896, 0.018795, 0.017730, 0.016703, 0.015718, 0.014774,
			0.013872, 0.013013, 0.012196, 0.011420, 0.010684, 0.009989, 0.009331, 0.008711,
			0.008127, 0.007577, 0.007061, 0.006575, 0.006120, 0.005693, 0.005294, 0.004920,
			0.004570, 0.004244, 0.003939, 0.003655, 0.003389, 0.003142, 0.002912, 0.002698,
			0.002499, 0.002313, 0.002141, 0.001981, 0.001833, 0.001695, 0.001567, 0.001448
};

/* ---------------- SWITCH DEFINITIONS ---------------- */
#define SW_LSB  (1 << 10)   /* P0.10 */
#define SW_MSB  (1 << 12)   /* P0.12 */

/* ---------------- GLOBAL BUFFERS ---------------- */
float X[N], Y[N], state[K - 1];
float *h_active;

/* ---------------- PROTOTYPES ---------------- */
void PLLInit(void);
void UART0_Init(void);
void UART0_SendString(char *);
void DACInit(void);
void DAC_Write(float);
void Delay_Ms(unsigned int);
void Switch_Init(void);

/* ---------------- MAIN ---------------- */
int main(void) {

    int n, k;
    unsigned int prev_mode = 0xFF;

    PLLInit();
    UART0_Init();
    DACInit();
    Switch_Init();

    UART0_SendString("FIR Filter Demo Started\r\n");

    for (n = 0; n < N; n++)
        X[n] = sin(M_PI * n / 128) + sin(M_PI * n / 4);

    while (1) {

        /* ACTIVE-LOW switches */
        unsigned int sw1 = (IO0PIN & SW_LSB) ? 0 : 1;
        unsigned int sw2 = (IO0PIN & SW_MSB) ? 0 : 1;
        unsigned int mode = (sw2 << 1) | sw1;

        if (mode != prev_mode) {
            switch (mode) {
                case 0:
										UART0_SendString("Mode: INPUT SIGNAL\r\n");
										break;
                case 1:
                    UART0_SendString("Mode: INPUT SIGNAL\r\n");
                    break;
                case 2:
                    UART0_SendString("Mode: HIGH PASS FILTER\r\n");
                    break;
                case 3:
                    UART0_SendString("Mode: LOW PASS FILTER\r\n");
                    break;
            }
            prev_mode = mode;
        }

        if (mode == 2) h_active = h_low;
        if (mode == 3) h_active = h_high;

        if (mode == 2 || mode == 3) {
            memset(state, 0, sizeof(state));
            for (n = 0; n < N; n++) {
                float sum = h_active[0] * X[n];
                for (k = 1; k < K; k++)
                    sum += h_active[k] * state[k - 1];
                Y[n] = sum;
                for (k = K - 2; k > 0; k--)
                    state[k] = state[k - 1];
                state[0] = X[n];
            }
        }

        for (k = 0; k < N; k++) {
            DAC_Write((mode >= 2) ? Y[k] : X[k]);
            Delay_Ms(2);
        }
    }
}

/* ---------------- SUPPORT ---------------- */

void Switch_Init(void) {
    PINSEL0 &= ~((3 << 20) | (3 << 24));
    IO0DIR  &= ~(SW_LSB | SW_MSB);
}

void DACInit(void) {
    PINSEL1 |= (1 << 19);
    PINSEL1 &= ~(1 << 18);
}

void DAC_Write(float v) {
    unsigned int d = (unsigned int)((v + 2.5) * 200);
    if (d > 1023) d = 1023;
    DACR = d << 6;
}

void Delay_Ms(unsigned int ms) {
    unsigned int i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 6000; j++);
}

void PLLInit(void) {
    PLLCON = 1; PLLCFG = 0x24;
    PLLFEED = 0xAA; PLLFEED = 0x55;
    while (!(PLLSTAT & (1 << 10)));
    PLLCON = 3;
    PLLFEED = 0xAA; PLLFEED = 0x55;
    VPBDIV = 1;
}

void UART0_Init(void) {
    PINSEL0 |= 0x05;
    U0LCR = 0x83;
    U0DLM = 1; U0DLL = 0x87;
    U0LCR = 3;
}

void UART0_SendString(char *s) {
    while (*s) {
        while (!(U0LSR & 0x20));
        U0THR = *s++;
    }
}
