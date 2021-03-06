#include "Oscilloscope.h"

// VIDI-X mod - dodano za freqvenciju
////////////
////////////
unsigned long chrono, chrono1;
unsigned long sampling_period_us;
byte peak[SAMPLES2] = {0};
float complex datae[SAMPLES2];
int sound[SAMPLES2];
float MULT = MAX_FREQ * 1000.0 / SAMPLES2;
unsigned int P2P = 0;
int LOG2SAMPLE = log(SAMPLES2) / log(2);

typedef enum fft_dir {
  FFT_FORWARD,    /* kernel uses "-1" sign */
  FFT_INVERSE     /* kernel uses "+1" sign */
} fft_dir;

void ffti_shuffle_f(float complex *datae, unsigned int log2_N)
{
  unsigned int N = 1 << log2_N;   /* N */
  unsigned int Nd2 = N >> 1;      /* N/2 = number range midpoint */
  unsigned int Nm1 = N - 1;       /* N-1 = digit mask */
  unsigned int i;                 /* index for array elements */
  unsigned int j;                 /* index for next element swap location */

  for (i = 0, j = 0; i < N; i++) {
    if (j > i) {
      float complex tmp = datae[i];
      datae[i] = datae[j];
      datae[j] = tmp;
    }

    unsigned int lszb = ~i & (i + 1);
    unsigned int mszb = Nd2 / lszb;
    unsigned int bits = Nm1 & ~(mszb - 1);
    j ^= bits;
  }
}

void ffti_evaluate_f(float complex *datae, unsigned int log2_N, fft_dir direction)
{
  unsigned int N = 1 << log2_N;
  double theta_2pi = (direction == FFT_FORWARD) ? -PI : PI;
  theta_2pi *= 2;

  for (unsigned int r = 1; r <= log2_N; r++)
  {
    unsigned int m = 1 << r;
    unsigned int md2 = m >> 1;
    double theta = theta_2pi / m;
    double re = cos(theta);
    double im = sin(theta);
    double complex Wm = re + I * im;
    for (unsigned int n = 0; n < N; n += m) {
      double complex Wmk = 1.0f;   /* Use double for precision */
      for (unsigned int k = 0; k < md2; k++) {
        unsigned int i_e = n + k;
        unsigned int i_o = i_e + md2;
        double complex t = Wmk * datae[i_o];
        datae[i_o] = datae[i_e] - t;
        datae[i_e] = datae[i_e] + t;
        t = Wmk * Wm;
        Wmk = t;
      }
    }
  }
}

void ffti_f(float complex datae[], unsigned int log2_N, fft_dir direction)
{
  ffti_shuffle_f(datae, log2_N);
  ffti_evaluate_f(datae, log2_N, direction);
}
////////////
////////////

///////////////////////////////////////////////////////////////////////////////////////////////
 bool Gen = true;

void OscilloscopeClass::DrawText()
{
	GO.Lcd.setTextColor(WHITE, BLACK);
	GO.Lcd.fillRect(272, last_menu, 68, 10, BLACK);
	GO.Lcd.fillRect(272, menu, 68, 10, BLUE);
	GO.Lcd.drawString((Start ? "Run" : "Stop"), 272, 20, 1);
	GO.Lcd.drawString(String(String(Ranges[range0]) + "/DIV"), 272, 30, 1);
	GO.Lcd.drawString(String(String(Ranges[range1]) + "/DIV"), 272, 40, 1);
	GO.Lcd.drawString(String(String(Rates[rate]) + "/DIV"), 272, 50, 1);
	GO.Lcd.drawString(String(Modes[ch0_mode]), 272, 60, 1);
	GO.Lcd.drawString(String(Modes[ch1_mode]), 272, 70, 1);
	GO.Lcd.drawString(String("OF1:" + String(ch0_off)), 272, 80, 1);
	GO.Lcd.drawString(String("OF2:" + String(ch1_off)), 272, 90, 1);
	GO.Lcd.drawString(String(trig_ch == 0 ? "T Ch: 1" : "T Ch: 2"), 272, 100, 1);
	GO.Lcd.drawString(String(TRIG_Modes[trig_mode]), 272, 110);
	GO.Lcd.drawString(String("T.LVL:" + String(trig_lv - 30)), 272, 120, 1);
	GO.Lcd.drawString(String((trig_edge == TRIG_E_UP) ? "T.E: UP" : "T.E: DN"), 272, 130, 1);
	GO.Lcd.drawString(String("Gen:" + String(Generator[Gen])), 272, 140, 1);
	//GO.Lcd.drawString(String("F:" + String(freqmax) + "         "), 262, 5, 1); //VIDI-X mod
	GO.Lcd.setTextColor(WHITE, BLACK);
}

void OscilloscopeClass::CheckSW()
{
	GO.update();
	if (GO.JOY_Y.wasAxisPressed() == 1)
	{
		last_menu = menu;
		(menu < 130) ? (menu += 10) : (menu = 19);
		DrawText();
		return;
	}

	else if (GO.JOY_Y.wasAxisPressed() == 2)
	{
		last_menu = menu;
		(menu > 19) ? (menu -= 10) : (menu = 130);
		DrawText();
		return;
	}

	else if (GO.JOY_X.wasAxisPressed() == 2)
	{
		switch (menu)
		{
		case 19:
			Start = !Start;
			trig = false;
			break;
		case 29:
			if (range0 < RANGE_MAX)
			{
				range0++;
			}
			break;
		case 39:
			if (range1 < RANGE_MAX)
			{
				range1++;
			}
			break;
		case 49:
			if (rate > 0)
			{
				rate--;
			}
			break;
		case 59:
			(ch0_mode > MODE_ON) ? (ch0_mode--) : (ch0_mode = MODE_OFF);
			break;
		case 69:
			(ch1_mode > MODE_ON) ? (ch1_mode--) : (ch1_mode = MODE_OFF);
			break;
		case 79:
			if (ch0_off > 0)
			{
				ch0_off -= 4096 / VREF[range0];
				while ((GO.JOY_X.isAxisPressed() == 2) && ch0_off > 0)
				{
					ch0_off -= 4096 / VREF[range0];
					delay(100);
					GO.update();
					DrawText();
				}
			}
			break;
		case 89:
			if (ch1_off > 0)
			{
				ch1_off -= 4096 / VREF[range1];

				while ((GO.JOY_X.isAxisPressed() == 2) && ch1_off > 0)
				{
					ch1_off -= 4096 / VREF[range1];
					delay(100);
					GO.update();
					DrawText();
				}
			}
			break;
		case 99:
			trig_ch = !trig_ch;
			break;
		case 109:
			(trig_mode > TRIG_AUTO) ? (trig_mode--) : (trig_mode = TRIG_ONE);
			Start = true;
			trig = false;
			break;
		case 119:
			if (trig_lv > 30)
			{
				trig_lv--;
			}
			else
			{
				trig_lv = 130;
			}
			break;
		case 129:
			trig_edge = !trig_edge;
			break;
		case 139:
			Gen = !Gen;
			break;
		}
		DrawText();
		return;
	}
	if (GO.JOY_X.wasAxisPressed() == 1)
	{
		switch (menu)
		{
		case 19:
			Start = !Start;
			trig = false;
			break;
		case 29:
			if (range0 > 0)
			{
				range0--;
			}
			break;
		case 39:
			if (range1 > 0)
			{
				range1--;
			}
			break;
		case 49:
			if (rate < RATE_MAX)
			{
				rate++;
			}
			break;
		case 59:
			(ch0_mode < MODE_OFF) ? (ch0_mode++) : (ch0_mode = MODE_ON);
			break;
		case 69:
			(ch1_mode < MODE_OFF) ? (ch1_mode++) : (ch1_mode = MODE_ON);
			break;
		case 79:
			if (ch0_off < 4095)
			{
				ch0_off += 4096 / VREF[range0];
				while ((GO.JOY_X.isAxisPressed() == 1) && ch0_off < 4095)
				{
					ch0_off += 4096 / VREF[range0];
					delay(100);
					GO.update();
					DrawText();
				}
			}
			break;
		case 89:
			if (ch1_off < 4095)
			{
				ch1_off += 4096 / VREF[range1];
				while ((GO.JOY_X.isAxisPressed() == 1) && ch0_off < 4095)
				{
					ch1_off += 4096 / VREF[range1];
					delay(100);
					GO.update();
					DrawText();
				}
			}
			break;
		case 99:
			trig_ch = !trig_ch;
			break;
		case 109:
			(trig_mode < TRIG_ONE) ? (trig_mode++) : (trig_mode = TRIG_AUTO);
			Start = true;
			trig = false;
			break;
		case 119:
			if (trig_lv < 130)
			{
				trig_lv++;
			}
			else
			{
				trig_lv = 30;
			}
			break;
		case 129:
			trig_edge = !trig_edge;
			break;
		case 139:
			Gen = !Gen;
			break;
		}
		DrawText();
		return;
	}
	if (GO.BtnB.wasPressed())
	{
		exitprg = true;
	}
	else
	{
		return;
	}
}

void OscilloscopeClass::DrawGrid()
{
	for (int x = 0; x <= SAMPLES; x += 2) // Horizontal Line
	{
		for (int y = 0; y <= LCD_HEIGHT; y += DOTS_DIV)
		{
			GO.Lcd.drawPixel(x, y, OSCGREY);
		}
		if (LCD_HEIGHT == 240)
		{
			GO.Lcd.drawPixel(x, LCD_HEIGHT - 1, OSCGREY);
		}
	}
	for (int x = 0; x <= SAMPLES; x += DOTS_DIV) // Vertical Line
	{
		for (int y = 0; y <= LCD_HEIGHT; y += 2)
		{
			GO.Lcd.drawPixel(x, y, OSCGREY);
		}
	}
	CheckSW();
	DrawText();

// VIDI-X mod - dodano za freqvenciju
////////////////
////////////////
  for (int i = 0; i < SAMPLES2; i++) {
    unsigned long chrono = micros();
    datae[i] = adRead(&ad_ch0, &ch0_mode, &ch0_off);
    while (micros() - chrono < sampling_period_us); // do nothing
  }

  ffti_f(datae, LOG2SAMPLE, FFT_FORWARD);
  int nFreq = SAMPLES2 / 2;
  //int hauteur = LCD_HEIGHT;

  int decal = 15;
  // Affichage du spectre
  int ampmax = 0;
  int imax = 0;
  for (int i = 2; i < nFreq; i++) {
    int amplitude = (int)creal(datae[i]) / COEF;
    if (amplitude > ampmax) {
      ampmax = amplitude;
      imax = i;
    }
    //amplitude = min(amplitude, hauteur - decal);
    //display.drawFastVLine (i * 2 *2, hauteur - amplitude - decal-80, amplitude*2+1, TFT_GREEN);
  }

  // Affichage fr??quence de plus forte amplitude
  if (ampmax > 20) { // VIDI-X mod - dodano za freqvenciju
    //display.setTextSize(2);
    //display.setTextColor(TFT_BLUE, TFT_YELLOW);
    int freqmax = 985.0 * imax * MAX_FREQ / SAMPLES2;
	GO.Lcd.drawString(String("Freq:" + String(freqmax) + " Hz    "), 262, 5, 1); //VIDI-X mod
	//Serial.println(freqmax);
    //char texte[14];
    //sprintf(texte, "%4d Hz: %3d", freqmax, ampmax);
    //display.drawString(texte, display.width() - 145, 0, 1);
  }
  ////////////////
  ////////////////
}

void OscilloscopeClass::DrawGrid(int x)
{
	if ((x % 2) == 0)
	{
		for (int y = 0; y <= LCD_HEIGHT; y += DOTS_DIV)
		{
			GO.Lcd.drawPixel(x, y, OSCGREY);
		}
	}
	if ((x % DOTS_DIV) == 0)
	{
		for (int y = 0; y <= LCD_HEIGHT; y += 2)
		{
			GO.Lcd.drawPixel(x, y, OSCGREY);
		}
	}
}

void OscilloscopeClass::ClearAndDrawGraph()
{
	int clear = 0;

	if (sample == 0)
	{
		clear = 2;
	}
	for (int x = 0; x < (SAMPLES - 1); x++)
	{
		GO.Lcd.drawLine(x, LCD_HEIGHT - data[clear][x], x + 1, LCD_HEIGHT - data[clear][x + 1], BLACK);
		GO.Lcd.drawLine(x, LCD_HEIGHT - data[clear + 1][x], x + 1, LCD_HEIGHT - data[clear + 1][x + 1], BLACK);
		if (ch0_mode != MODE_OFF)
		{
			GO.Lcd.drawLine(x, LCD_HEIGHT - data[sample][x], x + 1, LCD_HEIGHT - data[sample][x + 1], CH1COLOR);
		}
		if (ch1_mode != MODE_OFF)
		{
			GO.Lcd.drawLine(x, LCD_HEIGHT - data[sample + 1][x], x + 1, LCD_HEIGHT - data[sample + 1][x + 1], CH2COLOR);
		}
	}
	DrawGrid();
}

void OscilloscopeClass::ClearAndDrawDot(int i)
{
	int clear = 0;

	if (i <= 1)
	{
		return;
	}
	if (sample == 0)
	{
		clear = 2;
	}
	if (ch0_mode != MODE_OFF)
	{
		GO.Lcd.drawLine(i - 1, LCD_HEIGHT - data[clear][i - 1], i, LCD_HEIGHT - data[clear][i], BLACK);
		GO.Lcd.drawLine(i - 1, LCD_HEIGHT - data[sample][i - 1], i, LCD_HEIGHT - data[sample][i], CH1COLOR);
	}
	if (ch1_mode != MODE_OFF)
	{
		GO.Lcd.drawLine(i - 1, LCD_HEIGHT - data[clear + 1][i - 1], i, LCD_HEIGHT - data[clear + 1][i], BLACK);
		GO.Lcd.drawLine(i - 1, LCD_HEIGHT - data[sample + 1][i - 1], i, LCD_HEIGHT - data[sample + 1][i], CH2COLOR);
	}
	DrawGrid(i);
}

const uint8_t gen_pin = 26;

void SigGen_Task(void *parameter)
{
	pinMode(gen_pin, OUTPUT);
	for (;;)
	{
		if (Gen) 
		{
			digitalWrite(gen_pin, !digitalRead(gen_pin));
		}
		delay(3);
	}
	vTaskDelete(NULL);
}


void OscilloscopeClass::Run()
{
	pinMode(ad_ch0, ANALOG);
	pinMode(ad_ch1, ANALOG);
	exitprg = false;
	GO.Lcd.fillScreen(BLACK);
	DrawGrid();

	if (Sig_Gen == NULL)
	{
		xTaskCreatePinnedToCore(
			SigGen_Task,		/* Task function. */
			"Signal Generator", /* name of the task, a name just for humans */
			1024,				/* Stack size of task */
			NULL,				/* parameter of the task */
			2,					/* priority of the task */
			&Sig_Gen,			/* Task handle to keep track of the created task */
			0);					/* cpu core number where the task is assigned*/
	}
	while (!GO.BtnB.wasPressed())
	{
		if (trig_mode != TRIG_SCAN)
		{
			unsigned long st = millis();
			if (trig_ch == 0)
			{
				oad = adRead(&ad_ch0, &ch0_mode, &ch0_off);
			}
			else
			{
				oad = adRead(&ad_ch1, &ch1_mode, &ch1_off);
			}
			for (;;)
			{
				if (trig_ch == 0)
				{
					ad = adRead(&ad_ch0, &ch0_mode, &ch0_off);
				}
				else
				{
					ad = adRead(&ad_ch1, &ch1_mode, &ch1_off);
				}

				if (trig_edge == TRIG_E_UP)
				{
					if (ad >= trig_lv && ad > oad)
					{
						break;
					}
				}
				else
				{
					if (ad <= trig_lv && ad < oad)
					{
						break;
					}
				}
				oad = ad;
				if (trig_mode == TRIG_SCAN)
				{
					break;
				}
				if (trig_mode == TRIG_AUTO && (millis() - st) > 100) //VIDI-X mod
				{
					break;
				}
				CheckSW();
			}
		}
		if (rate <= 6 && Start) // sample and draw depending on the sampling rate
		{
			if (sample == 0) // change the index for the double buffer
			{
				sample = 2;
			}
			else
			{
				sample = 0;
			}

			if (rate == 0) // full speed, channel 0 only
			{
				for (int i = 0; i < SAMPLES; i++)
				{
					data[sample][i] = adRead(&ad_ch0, &ch0_mode, &ch0_off);
				}
				for (int i = 0; i < SAMPLES; i++)
				{
					data[sample + 1][i] = 0;
				}
			}
			else if (rate == 1) // full speed, dual channel
			{
				for (int i = 0; i < SAMPLES; i++)
				{
					data[sample][i] = adRead(&ad_ch0, &ch0_mode, &ch0_off);
					data[sample + 1][i] = adRead(&ad_ch1, &ch1_mode, &ch1_off);
				}
			}
			else if (rate >= 2 && rate <= 6)
			{
				unsigned long st = micros();
				unsigned long r = r_1[rate - 2];
				for (int i = 0; i < SAMPLES; i++)
				{
					while ((st - micros()) < r)
					{
						portYIELD();
					}
					st += r;
					data[sample][i] = adRead(&ad_ch0, &ch0_mode, &ch0_off);
					data[sample + 1][i] = adRead(&ad_ch1, &ch1_mode, &ch1_off);
				}
			}
			ClearAndDrawGraph();
		}
		else if (Start) // 5ms - 500ms sampling
		{
			if (sample == 0)
			{
				for (int i = 0; i < SAMPLES; i++)
				{
					data[2][i] = data[0][i]; // copy currently showing data to another
					data[3][i] = data[1][i];
				}
			}
			else
			{
				for (int i = 0; i < SAMPLES; i++)
				{
					data[0][i] = data[2][i];
					data[1][i] = data[3][i];
				}
			}
			unsigned long st = micros();
			for (int i = 0; i < SAMPLES; i++)
			{
				while ((st - micros()) < r_2[rate - 7])
				{
					CheckSW();
					if (rate < 7)
					{
						break;
					}
				}
				if (rate < 7) // sampling rate has been changed
				{
					break;
				}
				st += r_2[rate - 7];
				if (st - micros() > r_2[rate - 7]) // sampling rate has been changed to shorter interval
				{
					st = micros();
				}
				if (!Start)
				{
					i--;
					continue;
				}
				data[sample][i] = adRead(&ad_ch0, &ch0_mode, &ch0_off);
				
				data[sample + 1][i] = adRead(&ad_ch1, &ch1_mode, &ch1_off);
				ClearAndDrawDot(i);



			}
		
			DrawGrid();
		}
		else
		{
			CheckSW();
		}

		if (trig_mode == TRIG_ONE && !trig)
		{
			Start = false;
			trig = true;
			DrawText();
		}
		
		if (exitprg) // endless loop exit condition
		{
			break;
		}
	}
}

OscilloscopeClass::OscilloscopeClass()
{
	GO.Lcd.setTextSize(1);
	GO.Lcd.setTextFont(1);

// VIDI-X mod - dodano za freqvenciju
////////////
////////////

	    sampling_period_us = round(1000ul * (1.0 / MAX_FREQ));
        chrono1 = millis();
////////////
////////////

}

OscilloscopeClass::~OscilloscopeClass()
{
	vTaskDelete(Sig_Gen);
	Sig_Gen = NULL;
	dacWrite(26, 0);
	GO.Lcd.fillScreen(BLACK);
	GO.show();
}