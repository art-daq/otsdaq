#ifndef _PixelHistoPicGen_h_
#define _PixelHistoPicGen_h_

#include <stdlib.h>
#include <string>
//#include "PixelHistoFPix.h"

class PixelHistoPicGen
{
  public:
	PixelHistoPicGen();
	~PixelHistoPicGen();

	void readBmpToReadImg(const std::string& filename);
	void convertBmp(const std::string& fileBMP, const std::string& convertFile);

	void initImgBuffer(int sourceKey);  //fpix buffer
	void initImgBuffer(int r, int g, int b);
	void writeImgToBmp(std::string filename);
	void transferReadImgToImg(void);
	void drawDiscToImg(int disc, bool isClicked = false);
	void drawPanelToImg(int disc, int panel, int x, int y, bool isClicked = false);
	void drawBladeToImg(int   disc,
	                    int   panel,
	                    int   halfDisc,
	                    int   blade,
	                    int   x,
	                    int   y,
	                    float deg,
	                    int&  roci,
	                    int&  clicki,
	                    bool  isClicked = false);
	void drawFillRectAng(int x, int y, int w, int h, int r, int g, int b, float deg);
	void drawFillRect(int   x,
	                  int   y,
	                  int   w,
	                  int   h,
	                  int   r,
	                  int   g,
	                  int   b,
	                  float m1 = 1.0f,
	                  float m2 = 0.0f,
	                  float m3 = 0.0f,
	                  float m4 = 1.0f);
	void setImgPixel(int x, int y, int r, int g, int b);
	void fillFPixColors(void);
	std::string getFPixStandardName(
	    int disc, int panel, int halfDisc, int blade, int roc);
	void getFPixIndices(
	    std::string stdName, int& disc, int& panel, int& halfDisc, int& blade, int& roc);

	void        initBImgBuffer(int sourceKey);  //bpix buffer
	void        initBImgBuffer(int r, int g, int b);
	void        writeBImgToBmp(std::string filename);
	void        transferReadImgToBImg(void);
	void        fillBPixColors(void);
	void        drawLayerToBImg(int layer, bool isClicked = false);
	void        drawBPixRoc(int x, int y, int r, int g, int b);
	std::string getBPixStandardName(int layer, int pm, int row, int col);
	void getBPixIndices(std::string stdName, int& layer, int& pm, int& row, int& col);

	//common
	void setRocColor(std::string stdName, int r, int g, int b);
	void setRocColor(std::string stdName, bool isGood);

	//auxiliary buffer
	void clearAuxBuffer(int r, int g, int b, int a);
	void drawFillRectAux(int   x,
	                     int   y,
	                     int   w,
	                     int   h,
	                     int   r,
	                     int   g,
	                     int   b,
	                     float m1 = 1.0f,
	                     float m2 = 0.0f,
	                     float m3 = 0.0f,
	                     float m4 = 1.0f);
	void drawFillRectAngAux(int x, int y, int w, int h, int r, int g, int b, float deg);
	void setAuxPixel(int x, int y, int r, int g, int b);
	void writeAuxToBmp(char* fn);
	void createRocAlphaMasks(void);  //added 2nd trip

	void PrepareDetectorNavigatorHtml(void);  //web page essentials
	void PrepareDetectorNavigatorJava(void);

	void createClickMask(std::string maskString, int size);
	void createAuxImages(void);  // generates pre-made images for web-client

	void clearTurtleBuffer(int r, int g, int b, int a);
	void writeTurtleToBmp(const char* fn);
	void generateTurtle(const std::string& filepath);
	void resetFPixGood(int disk);
	int  getFPixGood(int disk);
	void resetBPixGood(int part);
	int  getBPixGood(int part);

	enum
	{  //public for user enums here

		IMG_INIT_BLACK = 0,
		IMG_INIT_BGIMG = 1,

		COLOR_HI_R = 255,
		COLOR_HI_G = 255,
		COLOR_HI_B = 255,
		COLOR_LO_R = 150,
		COLOR_LO_G = 150,
		COLOR_LO_B = 150,
	};

	bool imageMagickInstallChecked = false;
	bool imageMagickInstalled      = false;

  private:
	enum
	{
		//fpix enums
		IMG_DIM_MULT    = 1,
		IMG_WIDTH       = 600 * IMG_DIM_MULT,  //1200*IMG_DIM_MULT,
		IMG_HEIGHT      = 600 * IMG_DIM_MULT,
		IMG_HEADER_SIZE = 54,
		IMG_RAW_SIZE    = IMG_WIDTH * IMG_HEIGHT * 3,
		IMG_FILE_SIZE   = IMG_RAW_SIZE + IMG_HEADER_SIZE,

		DET_DISCL_X     = 300 * IMG_DIM_MULT,
		DET_DISCL_Y     = 300 * IMG_DIM_MULT,
		DET_DISCR_X     = 900 * IMG_DIM_MULT,
		DET_DISCR_Y     = 300 * IMG_DIM_MULT,
		DET_DISC_RADIUS = 295 * IMG_DIM_MULT,
		DET_ROC_SIZE    = 9 * IMG_DIM_MULT,
		DET_ROC_OFFSET  = 3 * IMG_DIM_MULT,
		DET_PLAQ_OFFSET = 4 * IMG_DIM_MULT,

		//bpix enums
		BIMG_DIM_MULT    = 1,
		BIMG_WIDTH       = 300 * BIMG_DIM_MULT,  //600*BIMG_DIM_MULT,
		BIMG_HEIGHT      = 600 * BIMG_DIM_MULT,
		BIMG_HEADER_SIZE = 54,
		BIMG_RAW_SIZE    = BIMG_WIDTH * BIMG_HEIGHT * 3,
		BIMG_FILE_SIZE   = BIMG_RAW_SIZE + BIMG_HEADER_SIZE,

		BPIX_LYRM_XOFF  = 7,
		BPIX_LYRM_YOFF  = 5,
		BPIX_LYRP_XOFF  = 307,
		BPIX_LYRP_YOFF  = 5,
		BPIX_ROC_WIDTH  = 7,
		BPIX_ROC_HEIGHT = 5,
		BPIX_COL_OFFSET = 2,
		BPIX_ROW_OFFSET = 2,
		BPIX_LYR1_ROWS  = 36,
		BPIX_LYR2_ROWS  = 60,
		BPIX_LYR3_ROWS  = 84,
		BPIX_LYR_COLS   = 32,

		//web enums
		WEB_LARGE_WIDTH   = IMG_WIDTH / IMG_DIM_MULT,
		WEB_LARGE_HEIGHT  = IMG_HEIGHT / IMG_DIM_MULT,
		WEB_MINI_WIDTH    = WEB_LARGE_WIDTH / 4,
		WEB_MINI_HEIGHT   = WEB_LARGE_HEIGHT / 4,
		WEB_BLARGE_WIDTH  = WEB_LARGE_WIDTH,
		WEB_BLARGE_HEIGHT = WEB_LARGE_HEIGHT,
		WEB_BMINI_WIDTH   = WEB_BLARGE_WIDTH / 6,
		WEB_BMINI_HEIGHT  = WEB_BLARGE_HEIGHT / 3,
		WEB_XOFFSET       = 0,
		WEB_YOFFSET       = 2,
		WEB_DISC_X        = DET_DISCL_X / IMG_DIM_MULT,
		WEB_DISC_Y        = DET_DISCL_Y / IMG_DIM_MULT,
		WEB_DISC_RADIUS   = DET_DISC_RADIUS / IMG_DIM_MULT,
		WEB_ROC_SIZE      = DET_ROC_SIZE / IMG_DIM_MULT,
		WEB_ROC_OFFSET    = DET_ROC_OFFSET / IMG_DIM_MULT,
		WEB_PLAQ_OFFSET   = DET_PLAQ_OFFSET / IMG_DIM_MULT,
		WEB_BLYR_XOFF     = BPIX_LYRM_XOFF * WEB_BLARGE_WIDTH / BIMG_WIDTH,
		WEB_BLYR_YOFF     = BPIX_LYRM_YOFF * WEB_BLARGE_HEIGHT / BIMG_HEIGHT,
		WEB_BROC_WIDTH    = BPIX_ROC_WIDTH * WEB_BLARGE_WIDTH / BIMG_WIDTH,
		WEB_BROC_HEIGHT   = BPIX_ROC_HEIGHT * WEB_BLARGE_HEIGHT / BIMG_HEIGHT,
		WEB_BCOL_OFFSET   = BPIX_COL_OFFSET * WEB_BLARGE_WIDTH / BIMG_WIDTH,
		WEB_BROW_OFFSET   = BPIX_ROW_OFFSET * WEB_BLARGE_HEIGHT / BIMG_HEIGHT,

		//colors
		COLOR_GOOD_R      = 0,
		COLOR_GOOD_G      = 255,
		COLOR_GOOD_B      = 0,
		COLOR_BAD_R       = 255,
		COLOR_BAD_G       = 0,
		COLOR_BAD_B       = 0,
		COLOR_INIT_R      = 50,
		COLOR_INIT_G      = 50,
		COLOR_INIT_B      = 50,
		COLOR_HIGHLIGHT_R = 255,
		COLOR_HIGHLIGHT_G = 255,
		COLOR_HIGHLIGHT_B = 100,
		CLICK_MASK_R      = 30,
		CLICK_MASK_G      = 30,
		CLICK_MASK_B      = 30,

		//highlight enums
		AUX_IMG_WIDTH       = WEB_ROC_SIZE * 4 + 1,
		AUX_IMG_HEIGHT      = AUX_IMG_WIDTH,
		AUX_IMG_HEADER_SIZE = 108,
		AUX_IMG_RAW_SIZE    = AUX_IMG_WIDTH * AUX_IMG_HEIGHT * 4,
		AUX_IMG_FILE_SIZE   = AUX_IMG_RAW_SIZE + AUX_IMG_HEADER_SIZE + 14,

		//turtle
		TUR_IMG_WIDTH       = 256,
		TUR_IMG_HEIGHT      = 256,
		TUR_IMG_HEADER_SIZE = 108,
		TUR_IMG_RAW_SIZE    = TUR_IMG_WIDTH * TUR_IMG_HEIGHT * 4,
		TUR_IMG_FILE_SIZE   = TUR_IMG_RAW_SIZE + TUR_IMG_HEADER_SIZE + 14,
	};

	//int *getIndices(string name);
	unsigned char& bpix_(int layer, int pm, int row, int col, int rgb)
	{
		switch(layer)
		{
		case 0:
			return bpixLyr1_[pm][row][col][rgb];
		case 1:
			return bpixLyr2_[pm][row][col][rgb];
		case 2:
			return bpixLyr3_[pm][row][col][rgb];
		default:
			exit(0);
		}
	}

	//fpix_ [disc]   [panel] [halfDisc] [blade] [roc] [rgb];
	unsigned char fpix_[4][2][2][12][24][3];
	int           fpixGood_[8];
	int           fpixOn_[8];
	int           bpixGood_[6];
	int           bpixOn_[6];

	//bpix_ [shells] [sectors] [layers] [ladders] [modules] [rocs];
	//layer   [p/m] [rows]  				 [cols] 				 [rgb];
	unsigned char bpixLyr1_[2][BPIX_LYR1_ROWS][BPIX_LYR_COLS][3];
	unsigned char bpixLyr2_[2][BPIX_LYR2_ROWS][BPIX_LYR_COLS][3];
	unsigned char bpixLyr3_[2][BPIX_LYR3_ROWS][BPIX_LYR_COLS][3];

	int* clickMask_;  //array of roc indices that have been clicked

	unsigned char img_[IMG_WIDTH][IMG_HEIGHT][3];  //RGB img representation {x,y,rgb}
	unsigned char bimg_[BIMG_WIDTH][BIMG_HEIGHT]
	                   [3];  //barrel RGB img representation {x,y,rgb}
	unsigned char auxImg_[AUX_IMG_WIDTH][AUX_IMG_HEIGHT]
	                     [4];   //RGB alpha img representation {x,y,rgba}
	unsigned char*** readImg_;  //RGB read img representation {x,y,rgb}
	int              readImgW_, readImgH_;

	unsigned char turtleImg_[TUR_IMG_WIDTH][TUR_IMG_HEIGHT]
	                        [4];  //RGB alpha img representation {x,y,rgba}
	bool firstTurtle;
};

#endif
