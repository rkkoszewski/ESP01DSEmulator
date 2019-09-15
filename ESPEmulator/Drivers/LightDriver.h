// Light Driver Interface
// Default Light Status: Off, Saturation 255;255;255, Sectors All Enabled, Brightness 100

#ifndef LightDriver_h
#define LightDriver_h

class LightDriver
{
  public:
    void setup();
    void setOn();
	void setOff();
	void setSaturation(byte r_limit, byte g_limit, byte b_limit);
	void setEnabledSectors(word enabled_sectors);
	void setColor(byte r, byte g, byte b);
	void setSectorColor();
	void setBrightness(byte percent);
	void update();
};

#endif