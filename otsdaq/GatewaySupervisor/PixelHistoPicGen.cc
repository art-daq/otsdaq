#include "PixelHistoPicGen.h"
#include <math.h>
#include <fstream>
#include <iostream>
#include <sstream>

#include <stdlib.h>

#define PI 3.14159265

using namespace std;

////////////////////////////////////////////////////////////////////////
///handles rotation operations
void transform(float& x, float& y, float m1, float m2, float m3, float m4)
{
	float tx, ty;

	tx = x * m1 + y * m2;
	ty = x * m3 + y * m4;

	x = tx;
	y = ty;
}

//================================================================================
//================================================================================
//================================================================================

////////////////////////////////////////////////////////////////////////
PixelHistoPicGen::PixelHistoPicGen()
{
	std::string mthn = "[PixelHistoPicGen::PixelHistoPicGen()]\t";

	readImg_   = 0;
	clickMask_ = 0;

	firstTurtle = true;
}

////////////////////////////////////////////////////////////////////////
PixelHistoPicGen::~PixelHistoPicGen()
{
	//dealloc
	if(readImg_)
	{
		for(int x = 0; x < readImgW_; ++x)
		{
			for(int y = 0; y < readImgH_; ++y)
				delete[] readImg_[x][y];
			delete[] readImg_[x];
		}
		delete[] readImg_;
		readImg_ = 0;
	}
}

/// ////////////////////////////////////////////////////////////////////////
/// //default(0)-is all black,
/// void PixelHistoPicGen::initImgBuffer(int sourceKey)
// {
// 	switch(sourceKey){
// 		case 1:  //FPix disc bg img
//   		readBmpToReadImg("/tmp/detector.bmp");
//   		transferReadImgToImg();
//   		break;
// 		default:
//   		for(int x=0;x<IMG_WIDTH;++x)
// 				for(int y=0;y<IMG_HEIGHT;++y){
// 				  img_[x][y][0] = 0;
// 				  img_[x][y][1] = 0;
// 				  img_[x][y][2] = 0;
// 				}
//   }
// }

/// ////////////////////////////////////////////////////////////////////////
/// //default(0)-is all black,
/// void PixelHistoPicGen::initBImgBuffer(int sourceKey)
// {
// 	switch(sourceKey){
// 		case 1:  //BPix layer bg img
//   		readBmpToReadImg("/tmp/detector.bmp");
//   		transferReadImgToBImg();
//   		break;
// 		default:
//   		for(int x=0;x<BIMG_WIDTH;++x)
// 				for(int y=0;y<BIMG_HEIGHT;++y){
// 				  bimg_[x][y][0] = 0;
// 				  bimg_[x][y][1] = 0;
// 				  bimg_[x][y][2] = 0;
// 				}
//   }
// }

/// ////////////////////////////////////////////////////////////////////////
/// void PixelHistoPicGen::initImgBuffer(int r,int g,int b)
// {
// 	for(int x=0;x<IMG_WIDTH;++x)
// 		for(int y=0;y<IMG_HEIGHT;++y){
// 		  img_[x][y][0] = r;
// 		  img_[x][y][1] = g;
// 		  img_[x][y][2] = b;
// 		}
// }

/// ////////////////////////////////////////////////////////////////////////
/// void PixelHistoPicGen::initBImgBuffer(int r,int g,int b)
// {
// 	for(int x=0;x<BIMG_WIDTH;++x)
// 		for(int y=0;y<BIMG_HEIGHT;++y){
// 		  bimg_[x][y][0] = r;
// 		  bimg_[x][y][1] = g;
// 		  bimg_[x][y][2] = b;
// 		}
// }

////////////////////////////////////////////////////////////////////////
///read bmp file to readImg_ buffer
void PixelHistoPicGen::readBmpToReadImg(const string& filename)
{
	//   string mthn = "[PicGen::readBMPToReadImg()]\t";

	ifstream file(filename.c_str(), ifstream::binary);
	if(!file.is_open())
		return;

	//   cout << mthn << "Has file." << endl;

	//BMP Header
	char buffer[64];
	file.read(buffer, 2);  //"BM"
	unsigned int size;
	file.read((char*)(&size), 4);
	//   cout << mthn << "size: " << size << endl;
	file.read(buffer, 4);
	unsigned int data_offset;
	file.read((char*)(&data_offset), 4);
	//   cout << mthn << "doff: " << data_offset << endl;
	unsigned int header_size;
	file.read((char*)(&header_size), 4);
	//   cout << mthn << "hsize: " << header_size << endl;
	unsigned int w, h;
	file.read((char*)(&w), 4);
	file.read((char*)(&h), 4);
	//   cout << mthn << w << " " << h << endl;
	file.read(buffer, 2);
	unsigned int depth = 0;
	file.read((char*)(&depth), 2);
	//   cout << mthn << depth << endl;
	if(depth != 24 && depth != 32)
	{
		file.close();
		return;
	}
	//   cout << mthn << "Depth correct." << endl;
	unsigned int compr_method;
	file.read((char*)(&compr_method), 4);
	//   cout << mthn << "method: " << compr_method << endl;

	//BMP Data
	file.seekg(data_offset, ios_base::beg);  //set file position to start of data

	//dealloc old
	if(readImg_)
	{
		for(int x = 0; x < readImgW_; ++x)
		{
			for(int y = 0; y < readImgH_; ++y)
				delete[] readImg_[x][y];
			delete[] readImg_[x];
		}
		delete[] readImg_;
		readImg_ = 0;
	}

	//allocate
	readImgW_ = w;
	readImgH_ = h;
	readImg_  = new unsigned char**[readImgW_];
	for(int x = 0; x < readImgW_; ++x)
	{
		readImg_[x] = new unsigned char*[readImgH_];
		for(int y = 0; y < readImgH_; ++y)
			readImg_[x][y] = new unsigned char[3];
	}

	//read data
	for(int y = readImgH_ - 1; y >= 0; --y)
		for(int x = 0; x < readImgW_; ++x)
		{
			for(int i = 2; i >= 0; --i)
				file.read((char*)(&(readImg_[x][y][i])), 1);
			if(depth == 32)                    //skip alpha byte
				file.seekg(1, ios_base::cur);  //skip one byte
		}

	file.close();
}

/// ////////////////////////////////////////////////////////////////////////
/// //transfer readImg_ buffer to img_ buffer
/// void PixelHistoPicGen::transferReadImgToImg()
// {
//   cout << "[PicGen::transferRimgToImg()]\t" << IMG_WIDTH << " " << IMG_HEIGHT << " " << readImgW_ << " " << readImgH_ << endl;
//   if(IMG_WIDTH != readImgW_ && IMG_HEIGHT != readImgH_){
//     cout << "[PicGen::transferRimgToImg()]\tInvalid dimensions." << endl;
//     return;
//   }
//   for(int x=0;x<IMG_WIDTH;++x)
//     for(int y=0;y<IMG_HEIGHT;++y){
//       img_[x][y][0] = readImg_[x][y][0];
//       img_[x][y][1] = readImg_[x][y][1];
//       img_[x][y][2] = readImg_[x][y][2];
//     }
// }

/// ////////////////////////////////////////////////////////////////////////
/// //transfer readImg_ buffer to bimg_ buffer
/// void PixelHistoPicGen::transferReadImgToBImg()
// {
//   cout << "[PicGen::transferRimgToBImg()]\t" << BIMG_WIDTH << " " << BIMG_HEIGHT << " " << readImgW_ << " " << readImgH_ << endl;
//   if(BIMG_WIDTH != readImgW_ && BIMG_HEIGHT != readImgH_){
//     cout << "[PicGen::transferRimgToBImg()]\tInvalid dimensions." << endl;
//     return;
//   }
//   for(int x=0;x<BIMG_WIDTH;++x)
//     for(int y=0;y<BIMG_HEIGHT;++y){
//       bimg_[x][y][0] = readImg_[x][y][0];
//       bimg_[x][y][1] = readImg_[x][y][1];
//       bimg_[x][y][2] = readImg_[x][y][2];
//     }
// }

////////////////////////////////////////////////////////////////////////
///write img buffer to bmp file
void PixelHistoPicGen::writeImgToBmp(string filename)
{
	// BMP Header				Stores general information about the BMP file.
	// Bitmap Information (DIB header)	Stores detailed information about the bitmap image.
	// Color Palette			Stores the definition of the colors being used for indexed color bitmaps.
	// Bitmap Data				Stores the actual image, pixel by pixel.

	ofstream file(filename.c_str(), ofstream::binary);
	if(!file.is_open())
		return;

	unsigned int bmpTemp;
	bmpTemp = IMG_FILE_SIZE;

	//BMP Header
	file << char(0x42) << char(0x4d);  //"BM"
	for(int i = 0; i < 4; ++i)
	{  //file size in little-endian
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	}
	file << char(0x00) << char(0x00) << char(0x00)
	     << char(0x00);  //0x00000000; //reserved bytes
	file << char(0x36) << char(0x00) << char(0x00)
	     << char(0x00);  //0x36000000; //offset to data
	file << char(0x28) << char(0x00) << char(0x00)
	     << char(0x00);  //0x28000000; //size of DIB header
	for(int i = 0, bmpTemp = IMG_WIDTH; i < 4; ++i)
	{  //img pixel width in little-endian
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	}
	for(int i = 0, bmpTemp = IMG_HEIGHT; i < 4; ++i)
	{  //img pixel height in little-endian
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	}
	file << char(0x01) << char(0x00) << char(0x18) << char(0x00) << char(0x00)
	     << char(0x00) << char(0x00) << char(0x00);  //details and 24-bit depth
	for(int i = 0, bmpTemp = IMG_RAW_SIZE; i < 4; ++i)
	{  //file raw data size in little-endian
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	}

	file << char(0x00) << char(0x00) << char(0x00) << char(0x00) << char(0x00)
	     << char(0x00) << char(0x00) << char(0x00) << char(0x00) << char(0x00)
	     << char(0x00) << char(0x00) << char(0x00) << char(0x00) << char(0x00)
	     << char(0x00);  //finish header details

	//BMP Data

	for(int y = IMG_HEIGHT - 1; y >= 0; --y)
		for(int x = 0; x < IMG_WIDTH; ++x)
			for(int i = 2; i >= 0; --i)
			{
				file << img_[x][y][i];
			}

	file.close();
}

////////////////////////////////////////////////////////////////////////
void PixelHistoPicGen::convertBmp(const std::string& fileBMP,
                                  const std::string& convertFile)
{
	string convertCmd = "convert " + fileBMP + " " + convertFile;
	system(convertCmd.c_str());
}

////////////////////////////////////////////////////////////////////////
void PixelHistoPicGen::drawFillRectAng(
    int x, int y, int w, int h, int r, int g, int b, float deg)
{
	float rad = deg * PI / 180.0f;
	drawFillRect(x, y, w, h, r, g, b, cos(rad), -sin(rad), sin(rad), cos(rad));
}

////////////////////////////////////////////////////////////////////////
///draws rect to img buffer. {x,y} is lower left corner. d is degrees of rotation around z-axis.
void PixelHistoPicGen::drawFillRect(int   x,
                                    int   y,
                                    int   w,
                                    int   h,
                                    int   r,
                                    int   g,
                                    int   b,
                                    float m1,
                                    float m2,
                                    float m3,
                                    float m4)
{
	float up[2] = {0.0f, 1.0f};

	transform(up[0], up[1], m1, m2, m3, m4);  //transform up vector

	float rt[2] = {up[1], -up[0]};  //get rt from up vector

	float px = (float)x;
	float py = (float)y;
	float tpx, tpy;

	for(int i = 0; i < w * 2; ++i)
	{
		tpx = px;
		tpy = py;
		for(float j = 0; j < h * 2; ++j)
		{
			setImgPixel((int)px, (int)py, r, g, b);
			px += up[0] / 2.0f;
			py += up[1] / 2.0f;
		}
		px = tpx + rt[0] / 2.0f;
		py = tpy + rt[1] / 2.0f;
	}
}

////////////////////////////////////////////////////////////////////////
void PixelHistoPicGen::setImgPixel(int x, int y, int r, int g, int b)
{
	img_[x][y][0] = r;
	img_[x][y][1] = g;
	img_[x][y][2] = b;
}

/// ////////////////////////////////////////////////////////////////////////
/// void PixelHistoPicGen::fillFPixColors()
// {
// 	//fpix_[4][2][2][12][24][3];
// 	for(int a=0;a<4;++a)
// 		for(int b=0;b<2;++b)
// 			for(int c=0;c<2;++c)
// 				for(int d=0;d<12;++d)
// 					for(int e=0;e<24;++e){
// 						fpix_[a][b][c][d][e][0] = COLOR_INIT_R;
// 						fpix_[a][b][c][d][e][1] = COLOR_INIT_G;
// 						fpix_[a][b][c][d][e][2] = COLOR_INIT_B;
// 					}
// }

/// ////////////////////////////////////////////////////////////////////////
/// void PixelHistoPicGen::setRocColor(string stdName,int rd,int gn,int bl)
// {
// 	if(stdName[0] == 'F'){
// 		int a,b,c,d,e;
// 		getFPixIndices(stdName,a,b,c,d,e);
// 		fpix_[a][b][c][d][e][0] = rd;
// 		fpix_[a][b][c][d][e][1] = gn;
// 		fpix_[a][b][c][d][e][2] = bl;
// 	}
// 	else if(stdName[0] == 'B'){
// 		int a,b,c,d;
// 		getBPixIndices(stdName,a,b,c,d);
// 		bpix_(a,b,c,d,0) = rd;
// 		bpix_(a,b,c,d,1) = gn;
// 		bpix_(a,b,c,d,2) = bl;
// 	}
// 	else
// 		cout << "PixelHistoPicGen::setRocColor()\tFailed." << endl;
// }


void recurseForBg(unsigned char*** d, int r, int c, int rm, int cm)
{
	if(r == rm || c == cm || r < 0 || c < 0)
		return;

	if(d[r][c][0] == 255 && d[r][c][1] == 0 && d[r][c][2] == 0)  //already found
		return;

	if(d[r][c][0] > 100)
	{  //white so mark & check for neighbors
		d[r][c][0] = 255;
		d[r][c][1] = 0;
		d[r][c][2] = 0;
		recurseForBg(d, r + 1, c, rm, cm);  //right
		recurseForBg(d, r - 1, c, rm, cm);  //left
		recurseForBg(d, r, c - 1, rm, cm);  //up
		recurseForBg(d, r, c + 1, rm, cm);  //dn
	}
}

#include <sys/stat.h>  // for mkdir
void PixelHistoPicGen::generateTurtle(const std::string& filepath)
{
	std::string tmpPath = filepath + "generated/tmp.bmp";
	int         offSetH;

	if(firstTurtle)
	{  //create first turtle
		firstTurtle = false;

		// attempt to make directory structure (just in case)
		mkdir((filepath + "generated").c_str(), 0755);

		readBmpToReadImg(filepath + "turtle.bmp");

		if(readImgW_ == 0)
			return;

		offSetH = (TUR_IMG_HEIGHT - readImgH_) / 2;

		clearTurtleBuffer(255, 255, 255, 255);

		recurseForBg(readImg_, 0, 0, readImgW_, readImgH_);
		recurseForBg(readImg_, 150, readImgH_ - 1, readImgW_, readImgH_);

		// std::cout << "readImgW_ " << readImgW_ << std::endl;
		// std::cout << "readImgH_ " << readImgH_ << std::endl;
		for(int i = 0; i < readImgW_; ++i)
			for(int j = 0; j < readImgH_; ++j)
			{
				if(readImg_[i][j][0] < 60 && readImg_[i][j][1] > 130)
				{
					turtleImg_[i][offSetH + j][0] = 0;
					turtleImg_[i][offSetH + j][1] = 0;
					turtleImg_[i][offSetH + j][2] = 255;
					//					cout << (int)readImg_[i][j][0] << " " << (int)readImg_[i][j][1] << " " <<	(int)readImg_[i][j][2] <<endl;
				}
				else
					for(int k = 0; k < 3; ++k)
						turtleImg_[i][offSetH + j][k] = readImg_[i][j][k];

				if(readImg_[i][j][0] == 255 && readImg_[i][j][1] == 0 &&
				   readImg_[i][j][2] == 0)
					turtleImg_[i][offSetH + j][3] = 0;  //invisible
				else
					turtleImg_[i][offSetH + j][3] = 255;
			}

		writeTurtleToBmp((filepath + "generated/turtleBase.bmp").c_str());
	}

	//change color
	readBmpToReadImg(filepath + "generated/turtleBase.bmp");
	offSetH = (TUR_IMG_HEIGHT - readImgH_) / 2;

	int rd = clock() % 256;
	int gn = (clock() / 3) % 256;
	int bl = (clock() * 3) % 256;

	for(int i = 0; i < readImgW_; ++i)
		for(int j = 0; j < readImgH_; ++j)
			if(readImg_[i][j][0] == 0 && readImg_[i][j][1] == 0 &&
			   readImg_[i][j][2] == 255)
			{
				turtleImg_[i][offSetH + j][0] = rd;
				turtleImg_[i][offSetH + j][1] = gn;
				turtleImg_[i][offSetH + j][2] = bl;
			}

	writeTurtleToBmp(tmpPath.c_str());
	convertBmp(tmpPath, filepath + "generated/turtle.png");
}  //end generateTurtle()

/// ////////////////////////////////////////////////////////////////////////
/// //creates the png's for the different angled ROC highlights for js mouseover
/// // and the good/bad boxes
/// void PixelHistoPicGen::createAuxImages()
// {
// 
// 		//create good/bad boxes
// 	char tmpPath[] = "images/generated/tmp.bmp";
// 
// 	clearAuxBuffer(COLOR_GOOD_R,COLOR_GOOD_G,COLOR_GOOD_B,0);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/good.png");
// 
// 	clearAuxBuffer(COLOR_BAD_R,COLOR_BAD_G,COLOR_BAD_B,0);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/bad.png");
// 
// 	clearAuxBuffer(COLOR_INIT_R,COLOR_INIT_G,COLOR_INIT_B,0);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/off.png");
// 
// 	clearAuxBuffer(0,0,0,255);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/invisible.png");
// 
// 	clearAuxBuffer(COLOR_HIGHLIGHT_R,COLOR_HIGHLIGHT_G,COLOR_HIGHLIGHT_B,0);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/rocHighlight.png");
// 
// 		//create alpha-background roc highlights
//   char convertPng[1000];
//   for(int i=0;i<6;++i){ //draw all 6 angles
//     clearAuxBuffer(0,0,0,255);
// 
//     drawFillRectAngAux(AUX_IMG_WIDTH/2+1,
//        AUX_IMG_HEIGHT/2+1,
//        WEB_ROC_SIZE*2+8,
//        WEB_ROC_SIZE*2+8,
//        COLOR_HIGHLIGHT_R,COLOR_HIGHLIGHT_G,COLOR_HIGHLIGHT_B,
//        7.5+i*15);
// 
//     writeAuxToBmp(tmpPath);
//     sprintf(convertPng,"images/generated/rocHighlight%d.png",i);
//   	convertBmp(tmpPath,convertPng);
//   }
// 
// 		//create summary color keys
// 	//for boolean
// 	for(int x=0;x<IMG_WIDTH;++x)
// 		for(int y=0;y<IMG_HEIGHT;++y)
// 		  setImgPixel(x,y,
// 				x>IMG_WIDTH/2?COLOR_GOOD_R:COLOR_BAD_R,
// 				x>IMG_WIDTH/2?COLOR_GOOD_G:COLOR_BAD_G,
// 				x>IMG_WIDTH/2?COLOR_GOOD_B:COLOR_BAD_B);
// 
// 	writeImgToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/summaryColorKeyBoolean.png");
// 
// 	//must be the same code as invoid PixelHistoViewer::colorRocsWithField(TTree *summary, string field) to match key
// 	int numOfColors = 6;
// 	int colors[6][3] = {
// 		{255,0,255},
// 		{0,0,255},
// 		{0,255,255},
// 		{0,255,0},
// 		{255,255,0},
// 		{255,0,0},
// 	};
// 
// 	float v;
// 	float sizeOfGrade = 1.0/(numOfColors-1);
// 	int ci;
// 
// 		//blended color key
// 	for(int x=0;x<IMG_WIDTH;++x)
// 		for(int y=0;y<IMG_HEIGHT;++y)
// 		{
// 
// 			v = (float)x/IMG_WIDTH;
// 			ci = (int)(v/sizeOfGrade);
// 			v -= ci*sizeOfGrade;
// 			v /= sizeOfGrade; //0-1
// 			setImgPixel(x,y,
// 				(int)(colors[ci][0]*(1-v) + colors[ci+1][0]*v),
// 				(int)(colors[ci][1]*(1-v) + colors[ci+1][1]*v),
// 				(int)(colors[ci][2]*(1-v) + colors[ci+1][2]*v));
// 		}
// 
// 	writeImgToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/summaryColorKey.png");
// 
// 		//for overflow
// 	clearAuxBuffer(COLOR_HI_R,COLOR_HI_G,COLOR_HI_B,0);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/summaryOverflowKey.png");
// 		//for underflow
// 	clearAuxBuffer(COLOR_LO_R,COLOR_LO_G,COLOR_LO_B,0);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/summaryUnderflowKey.png");
// }

////////////////////////////////////////////////////////////////////////
///initializes aux buffer to all invisible black pixels
void PixelHistoPicGen::clearTurtleBuffer(int r, int g, int b, int a)
{
	for(int x = 0; x < TUR_IMG_WIDTH; ++x)
		for(int y = 0; y < TUR_IMG_HEIGHT; ++y)
		{
			turtleImg_[x][y][0] = r;
			turtleImg_[x][y][1] = g;
			turtleImg_[x][y][2] = b;
			turtleImg_[x][y][3] = a;  //255 is invisible, 0 is opaque
		}
}

////////////////////////////////////////////////////////////////////////
void PixelHistoPicGen::writeTurtleToBmp(const char* fn)
{
	string mthn = "[PicGen::writeTurtleToBmp()]\t";

	// BMP Header				Stores general information about the BMP file.
	// Bitmap Information (DIB header)	Stores detailed information about the bitmap image.
	// Color Palette			Stores the definition of the colors being used for indexed color bitmaps.
	// Bitmap Data				Stores the actual image, pixel by pixel.

	ofstream file(fn, ofstream::binary);
	if(!file.is_open())
		return;

	unsigned int bmpTemp;

	//BMP Header
	file << char(0x42) << char(0x4d);  //"BM"
	bmpTemp = TUR_IMG_FILE_SIZE;
	for(int i = 0; i < 4; ++i)  //file size in little-endian
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //reserved bytes
	unsigned int dibHeaderSize = TUR_IMG_HEADER_SIZE + 14;
	for(int i = 0; i < 4; ++i)  //DIB header size
		file << (unsigned char)(((char*)(&dibHeaderSize))[i]);
	bmpTemp = TUR_IMG_HEADER_SIZE;
	for(int i = 0; i < 4; ++i)  //offset to data
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	bmpTemp = TUR_IMG_WIDTH;
	for(int i = 0; i < 4; ++i)  //img pixel width in little-endian
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	bmpTemp = TUR_IMG_HEIGHT;
	for(int i = 0; i < 4; ++i)  //img pixel height in little-endian
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	file << char(0x01) << char(0x00) << char(0x20)
	     << char(0x00);  //color planes and bits per pixel
	file << char(0x03) << char(0x00) << char(0x00) << char(0x00);  //compression method
	bmpTemp = TUR_IMG_RAW_SIZE;
	for(int i = 0; i < 4; ++i)  //file raw data size in little-endian
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	file << char(0xD6) << char(0x0D) << char(0x00)
	     << char(0x00);  //horiz resol in pixels per meter
	file << char(0xD6) << char(0x0D) << char(0x00)
	     << char(0x00);  //vert resol in pixels per meter
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //colors used
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //important colors
	file << char(0x00) << char(0x00) << char(0xFF) << char(0x00);  //red mask
	file << char(0x00) << char(0xFF) << char(0x00) << char(0x00);  //green mask
	file << char(0xFF) << char(0x00) << char(0x00) << char(0x00);  //blue mask
	file << char(0x00) << char(0x00) << char(0x00) << char(0xFF);  //alpha mask
	file << char(0x01) << char(0x00) << char(0x00) << char(0x00);  //color space
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //red X
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //red Y
	file << char(0x01) << char(0x00) << char(0x00) << char(0x00);  //red Z
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //green X
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //green Y
	file << char(0x01) << char(0x00) << char(0x00) << char(0x00);  //green Z
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //blue X
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //blue Y
	file << char(0x01) << char(0x00) << char(0x00) << char(0x00);  //blue Z
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //gamma red
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //gamma green
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //gamma blue

	//BMP Data

	int bytesToPad = (4 - TUR_IMG_WIDTH * 4 % 4) %
	                 4;  //each row must start on a multiple of 4 byte offset
	for(int y = TUR_IMG_HEIGHT - 1; y >= 0; --y)
	{
		for(int x = 0; x < TUR_IMG_WIDTH; ++x)
		{
			for(int i = 2; i >= 0; --i)
			{
				file << turtleImg_[x][y][i];
			}
			file << turtleImg_[x][y][3];  //alpha byte last
		}
		//pad bytes
		for(int p = 0; p < bytesToPad; ++p)
			file << char(0x00);
	}

	file.close();
}
////////////////////////////////////////////////////////////////////////
void PixelHistoPicGen::writeAuxToBmp(char* fn)
{
	string mthn = "[PicGen::writeAuxToBmp()]\t";

	// BMP Header				Stores general information about the BMP file.
	// Bitmap Information (DIB header)	Stores detailed information about the bitmap image.
	// Color Palette			Stores the definition of the colors being used for indexed color bitmaps.
	// Bitmap Data				Stores the actual image, pixel by pixel.

	ofstream file(fn, ofstream::binary);
	if(!file.is_open())
		return;

	unsigned int bmpTemp;

	//BMP Header
	file << char(0x42) << char(0x4d);  //"BM"
	bmpTemp = AUX_IMG_FILE_SIZE;
	for(int i = 0; i < 4; ++i)  //file size in little-endian
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //reserved bytes
	unsigned int dibHeaderSize = AUX_IMG_HEADER_SIZE + 14;
	for(int i = 0; i < 4; ++i)  //DIB header size
		file << (unsigned char)(((char*)(&dibHeaderSize))[i]);
	bmpTemp = AUX_IMG_HEADER_SIZE;
	for(int i = 0; i < 4; ++i)  //offset to data
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	bmpTemp = AUX_IMG_WIDTH;
	for(int i = 0; i < 4; ++i)  //img pixel width in little-endian
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	bmpTemp = AUX_IMG_HEIGHT;
	for(int i = 0; i < 4; ++i)  //img pixel height in little-endian
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	file << char(0x01) << char(0x00) << char(0x20)
	     << char(0x00);  //color planes and bits per pixel
	file << char(0x03) << char(0x00) << char(0x00) << char(0x00);  //compression method
	bmpTemp = AUX_IMG_RAW_SIZE;
	for(int i = 0; i < 4; ++i)  //file raw data size in little-endian
		file << (unsigned char)(((char*)(&bmpTemp))[i]);
	file << char(0xD6) << char(0x0D) << char(0x00)
	     << char(0x00);  //horiz resol in pixels per meter
	file << char(0xD6) << char(0x0D) << char(0x00)
	     << char(0x00);  //vert resol in pixels per meter
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //colors used
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //important colors
	file << char(0x00) << char(0x00) << char(0xFF) << char(0x00);  //red mask
	file << char(0x00) << char(0xFF) << char(0x00) << char(0x00);  //green mask
	file << char(0xFF) << char(0x00) << char(0x00) << char(0x00);  //blue mask
	file << char(0x00) << char(0x00) << char(0x00) << char(0xFF);  //alpha mask
	file << char(0x01) << char(0x00) << char(0x00) << char(0x00);  //color space
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //red X
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //red Y
	file << char(0x01) << char(0x00) << char(0x00) << char(0x00);  //red Z
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //green X
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //green Y
	file << char(0x01) << char(0x00) << char(0x00) << char(0x00);  //green Z
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //blue X
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //blue Y
	file << char(0x01) << char(0x00) << char(0x00) << char(0x00);  //blue Z
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //gamma red
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //gamma green
	file << char(0x00) << char(0x00) << char(0x00) << char(0x00);  //gamma blue

	//BMP Data

	int bytesToPad = (4 - AUX_IMG_WIDTH * 4 % 4) %
	                 4;  //each row must start on a multiple of 4 byte offset
	for(int y = AUX_IMG_HEIGHT - 1; y >= 0; --y)
	{
		for(int x = 0; x < AUX_IMG_WIDTH; ++x)
		{
			for(int i = 2; i >= 0; --i)
			{
				file << auxImg_[x][y][i];
			}
			file << auxImg_[x][y][3];  //alpha byte last
		}
		//pad bytes
		for(int p = 0; p < bytesToPad; ++p)
			file << char(0x00);
	}

	file.close();
}

////////////////////////////////////////////////////////////////////////
///DIFFERENT THAN drawFillRect... x,y is CENTER!!
///draws rect to aux img buffer. {x,y} is center. m1-4 is rotation matrix around z-axis.
void PixelHistoPicGen::drawFillRectAux(int   x,
                                       int   y,
                                       int   w,
                                       int   h,
                                       int   r,
                                       int   g,
                                       int   b,
                                       float m1,
                                       float m2,
                                       float m3,
                                       float m4)
{
	float up[2] = {0.0f, 1.0f};

	transform(up[0], up[1], m1, m2, m3, m4);  //transform up vector

	float rt[2] = {up[1], -up[0]};  //get rt from up vector

	float px = (float)x - up[0] * 0.5 * w -
	           rt[0] * 0.5 * w;  //subtract to get to bottom left corner
	float py = (float)y - up[1] * 0.5 * h - rt[1] * 0.5 * h;
	float tpx, tpy;

	for(int i = 0; i < w * 2; ++i)
	{
		tpx = px;
		tpy = py;
		for(float j = 0; j < h * 2; ++j)
		{
			setAuxPixel((int)px, (int)py, r, g, b);
			px += up[0] / 2.0f;
			py += up[1] / 2.0f;
		}
		px = tpx + rt[0] / 2.0f;
		py = tpy + rt[1] / 2.0f;
	}
}

////////////////////////////////////////////////////////////////////////
///DIFFERENT THAN drawFillRect... x,y is CENTER!!
///draws rect to aux img buffer. {x,y} is center. d is degrees of rotation around z-axis.
void PixelHistoPicGen::drawFillRectAngAux(
    int x, int y, int w, int h, int r, int g, int b, float deg)
{
	float rad = deg * PI / 180.0f;
	drawFillRectAux(x, y, w, h, r, g, b, cos(rad), -sin(rad), sin(rad), cos(rad));
}

////////////////////////////////////////////////////////////////////////
void PixelHistoPicGen::setAuxPixel(int x, int y, int r, int g, int b)
{
	auxImg_[x][y][0] = r;
	auxImg_[x][y][1] = g;
	auxImg_[x][y][2] = b;
	auxImg_[x][y][3] = 0;
}

