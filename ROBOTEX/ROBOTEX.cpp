#include "stdafx.h"
#include "GUICamera.h"
#include <Windows.h>

#define max3(r,g,b) ( r >= g ? (r >= b ? r : b) : (g >= b ? g : b) ) //max of three
#define min3(r,g,b) ( r <= g ? (r <= b ? r : b) : (g <= b ? g : b) ) //min of three

HANDLE readySignal = CreateEvent(NULL, FALSE, FALSE, NULL);
HANDLE getImageSignal = CreateEvent(NULL, FALSE,FALSE,NULL);
HANDLE setImageSignal = CreateEvent(NULL, FALSE, FALSE, NULL);
HANDLE button2Signal = CreateEvent(NULL, FALSE, FALSE, NULL);
HANDLE GUIThread;
extern BYTE *g_pBuffer;
extern float hueMin;
extern float hueMax;
extern float saturationMin;
extern float saturationMax;
extern float valueMin;
extern float valueMax;
DWORD *g_pBufferDW = (DWORD*)(&g_pBuffer);
BYTE *editBuffer;
BYTE *doubleBuffer;

void imageProcessingTest();
void calculateBrightness(BYTE* buffer);
void smoother(BYTE* buffer);
void calculateChroma(BYTE* buffer);
void calculateHue(BYTE* buffer);
void drawCross(int x, int y, int color, BYTE* buffer);
void kMeans(int k, int iterations, BYTE* buffer);
void calculateSaturation(BYTE* buffer);
void threshold(BYTE* buffer);

int main() {
	editBuffer = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 640 * 480 * 4);
	doubleBuffer = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 640 * 480 * 4);
	//Create the GUI in a separate thread
	GUIThread = CreateThread(NULL, 0, GUICamera, 0, 0, NULL);
	//Wait for the GUI to initialize
	WaitForSingleObject(readySignal, INFINITE);

	//TODO control the robot...
	prints(L"Testing printing\n"); //this function can print using the wprintf syntax
	imageProcessingTest();

	//Don't exit this thread before the GUI
	WaitForSingleObject(GUIThread, INFINITE);
}

void imageProcessingTest() {
	while (true) {
		//get the image after button 2 was pressed
		//WaitForSingleObject(button2Signal, INFINITE);
		SetEvent(getImageSignal);
		WaitForSingleObject(readySignal, INFINITE); //image has been copied to editBuffer
		CopyMemory(doubleBuffer, editBuffer, 640 * 480 * 4);
		//prints(L"here\n");
		//Filters:
		threshold(doubleBuffer);
		//calculateBrightness(editBuffer);
		//calculateHue(doubleBuffer);
		//calculateChroma(doubleBuffer);
		//calculateSaturation(editBuffer);
		kMeans(2,5, doubleBuffer);

		//display the image, copy editBuffer to the screen buffer g_pBuffer
		SetEvent(setImageSignal);
	}
}

//Y' from the YUV HDTV BT.709 standard
void calculateBrightness(BYTE* buffer) {
	for (DWORD *pixBuffer = (DWORD *)buffer; pixBuffer < 640 * 480 + (DWORD *)buffer;++pixBuffer) {
		DWORD pixel = *pixBuffer;
		float brightness = 0.2126f*(pixel & 0xFF) + 0.7152f*((pixel >> 8) & 0xFF) + 0.0722f*((pixel >> 16) & 0xFF);
		*pixBuffer = (int)brightness*(1 + (1 << 8) + (1 << 16)); //grayscale image of pixel values
	}
}

//calculates the Y' values module some number
void smoother(BYTE* buffer) {
	for (DWORD *pixBuffer = (DWORD *)buffer; pixBuffer < 640 * 480 + (DWORD *)buffer;++pixBuffer) {
		DWORD pixel = *pixBuffer;
		DWORD brightness = (int)(0.2126f*(pixel & 0xFF) + 0.7152f*((pixel >> 8) & 0xFF) + 0.0722f*((pixel >> 16) & 0xFF));
		brightness = brightness - brightness % 32;
		*pixBuffer = pixel*(1 + (1 << 8) + (1 << 16)); //grayscale image of pixel values
	}
}

void calculateChroma(BYTE* buffer) {
	for (DWORD *pixBuffer = (DWORD *)buffer; pixBuffer < 640 * 480 + (DWORD *)buffer;++pixBuffer) {
		DWORD pixel = *pixBuffer;
		DWORD chroma = max3((pixel & 0xFF), ((pixel >> 8) & 0xFF), ((pixel >> 16) & 0xFF))-
				min3((pixel & 0xFF), ((pixel >> 8) & 0xFF), ((pixel >> 16) & 0xFF));

		//chroma = chroma - chroma % 20;
		*pixBuffer = chroma*(1 + (1 << 8) + (1 << 16)); //grayscale image of pixel values

		//*pixBuffer = (chroma > 10) ? *pixBuffer : 0;
	}
}

void calculateSaturation(BYTE* buffer) {
	for (DWORD *pixBuffer = (DWORD *)buffer; pixBuffer < 640 * 480 + (DWORD *)buffer;++pixBuffer) {
		DWORD pixel = *pixBuffer;
		float saturation = (float)(max3((pixel & 0xFF), ((pixel >> 8) & 0xFF), ((pixel >> 16) & 0xFF)) - //chroma/value
			min3((pixel & 0xFF), ((pixel >> 8) & 0xFF), ((pixel >> 16) & 0xFF))) /
			(0.2126f*(pixel & 0xFF) + 0.7152f*((pixel >> 8) & 0xFF) + 0.0722f*((pixel >> 16) & 0xFF));

		//chroma = chroma - chroma % 20;
		*pixBuffer = (int)(saturation*255)*(1 + (1 << 8) + (1 << 16)); //grayscale image of pixel values

		//*pixBuffer = (saturation > 0.5) ? *pixBuffer : 0;
	}
}

void calculateHue(BYTE* buffer) {
	for (DWORD *pixBuffer = (DWORD *)buffer; pixBuffer < 640 * 480 + (DWORD *)buffer;++pixBuffer) {
		DWORD pixel = *pixBuffer;
		BYTE red = pixel & 0xFF, green = (pixel >> 8) & 0xFF, blue = (pixel >> 16) & 0xFF;
		float hue;

		//calculates hue in the range 0 to 6
		if(red >= green && green >= blue && red > blue)	hue =	((float)(green - blue)  /	(red - blue));
		else if (green > red && red >= blue)	hue =  (2 - (float)(red -	blue) /	(green - blue));
		else if (green >= blue && blue > red)	hue =  (2 + (float)(blue -	red)  /	(green - red));
		else if (blue > green && green > red)	hue =  (4 - (float)(green - red)   /	(blue - red));
		else if (blue > red && red >= green)	hue =  (4 + (float)(red - green)   /	(blue - green));
		else if (red >= blue && blue > green)	hue =  (6 - (float)(blue - green)  /	(red - green));
		else hue = 0; //Hue when the image is gray red=green=blue

		//pixel = 256 * hue / 6;
		//prints(L"%X %.2f \n", pixel, hue);
		//*pixBuffer = pixel*(1 + (1 << 8) + (1 << 16));
		*pixBuffer = (hue > 4 || hue < 2) ? *pixBuffer : 0;
	}
}

//k-Means algorithm, read on Wikipedia
void kMeans(int k, int iterations, BYTE* buffer) { //k centers, done for iterations iterations
	int *xCenter = new int[k], *yCenter = new int[k]; //centers of the k means
	DWORD *pixBuffer = (DWORD *)buffer;

	int *xNewCenter = new int[k], *yNewCenter = new int[k], *CenterCount = new int[k]; //new centers and count for averaging later
	
	//for (int i = 0; i < k; ++i) {	//distribute centers around the corners
	//	if ((i + 3) % 4 < 2)
	//		xCenter[i] = 640;
	//	else
	//		xCenter[i] = 0;
	//	if (i % 4 < 2)
	//		yCenter[i] = 0;
	//	else
	//		yCenter[i] = 480;
	//}

	for (int iterationCount = 0; iterationCount < iterations; ++iterationCount) {
		ZeroMemory(xNewCenter, sizeof(int)*k);
		ZeroMemory(yNewCenter, sizeof(int)*k);
		ZeroMemory(CenterCount, sizeof(int)*k);
		for (int currentY = 0; currentY < 480; ++currentY) {
			for (int currentX = 0; currentX < 640; ++currentX) {
				int pixel = 0xFF & pixBuffer[currentX + 640*currentY];
				int minDistanceSquared = (currentX-xCenter[0])*(currentX - xCenter[0])+ 
										 (currentY - yCenter[0])*(currentY - yCenter[0]);
				int minN = 0;
				for (int currentN = 1; currentN < k; ++currentN) {
					int DistanceSquared = (currentX - xCenter[currentN])*(currentX - xCenter[currentN]) +
										(currentY - yCenter[currentN])*(currentY - yCenter[currentN]);
					if (DistanceSquared < minDistanceSquared) {
						minN = currentN;
						minDistanceSquared = DistanceSquared;
					}
				}
				CenterCount[minN] += pixel;
				xNewCenter[minN] += pixel * currentX;
				yNewCenter[minN] += pixel * currentY;
			}
		}

		//all pixels looped, calculate new centers:
		for (int currentN = 0; currentN < k; ++currentN) {
			xCenter[currentN] = (int)((float)xNewCenter[currentN] / CenterCount[currentN]);
			yCenter[currentN] = (int)((float)yNewCenter[currentN] / CenterCount[currentN]);
		}
	}

	//draw crosses
	for (int currentN = 0; currentN < k; ++currentN) {
		drawCross(xCenter[currentN], yCenter[currentN], 0x00FFFFFF, editBuffer);
	}
	delete[] xCenter, yCenter, xNewCenter, yNewCenter, CenterCount;
}

void threshold(BYTE* buffer) {
	for (DWORD *pixBuffer = (DWORD*)buffer; pixBuffer < (DWORD*)buffer + 640*480; ++pixBuffer) {
		DWORD pixel = *pixBuffer;
		BYTE red = pixel & 0xFF, green = (pixel >> 8) & 0xFF, blue = (pixel >> 16) & 0xFF;

		float hue;
		//calculates hue in the range 0 to 6
		if (red >= green && green >= blue && red > blue)	hue = ((float)(green - blue) / (red - blue));
		else if (green > red && red >= blue)	hue = (2 - (float)(red - blue) / (green - blue));
		else if (green >= blue && blue > red)	hue = (2 + (float)(blue - red) / (green - red));
		else if (blue > green && green > red)	hue = (4 - (float)(green - red) / (blue - red));
		else if (blue > red && red >= green)	hue = (4 + (float)(red - green) / (blue - green));
		else if (red >= blue && blue > green)	hue = (6 - (float)(blue - green) / (red - green));
		else hue = 0; //Hue when the image is gray red=green=blue

		float saturation = (float)(max3(red, green, blue) - min3(red, green, blue))
			/ max3(red, green, blue);

		float value = (float)max3(red, green, blue) / 255;

		if (hue < hueMin || hue > hueMax || saturation < saturationMin || saturation > saturationMax ||
			value < valueMin || value > valueMax)
			*pixBuffer = 0;
	}
}

//draws a cross of the color #rrggbbaa to coordinates x, y starting from the bottom left corner (Windows bitmap uses that)
void drawCross(int x, int y, int color, BYTE* buffer) {
	DWORD *pixBuffer = (DWORD *)buffer;
	for (int j = -1;j <= 1;++j)
		for (int i = -10;i <= 10;++i)
			if(0 <= x+i && x+i < 640 && 0 <= y+j && y+j < 480)
				pixBuffer[x + i + 640 * (y+j)] = color;
	for (int j = -1;j <= 1;++j)
		for (int i = -10;i <= 10;++i)
			if (0 <= x + j && x+j < 640 && 0 <= y + i && y+i < 480)
				pixBuffer[x+j + 640 * (y+i)] = color;
}

void COMTesting() {
	HANDLE hComm = CreateFile(L"\\\\.\\COM12", GENERIC_READ | GENERIC_WRITE, 0, 0,
		OPEN_EXISTING, 0, 0);
	if (hComm == INVALID_HANDLE_VALUE)
		prints(L"INVALID HANDLE\n");

	char *writeBuffer = "s\n";
	DWORD bytesWritten;
	WriteFile(hComm, writeBuffer, 4, &bytesWritten, NULL);

	char *readBuffer[128]{};
	DWORD bytesRead;
	for (int i = 0;i < 10;++i) {
		ReadFile(hComm, readBuffer, 1, &bytesRead, NULL);
		prints(L"%s\n", readBuffer);
	}
}