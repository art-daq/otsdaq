#include "PixelHistoPicGen.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <math.h>

#include <stdlib.h>

#define PI 3.14159265

using namespace std;

////////////////////////////////////////////////////////////////////////
//handles rotation operations
void transform(float &x ,float &y, float m1, float m2, float m3, float m4){
  float tx,ty;

  tx = x * m1 + y * m2;
  ty = x * m3 + y * m4;

  x = tx;
  y = ty;
}
	
	
//================================================================================	
//================================================================================
//================================================================================
	
	
////////////////////////////////////////////////////////////////////////
PixelHistoPicGen::PixelHistoPicGen(){			
  	std::string mthn = "[PixelHistoPicGen::PixelHistoPicGen()]\t";

  	readImg_ = 0;
	clickMask_ = 0;
	
	firstTurtle = true;
}

////////////////////////////////////////////////////////////////////////
PixelHistoPicGen::~PixelHistoPicGen()
{
  //dealloc
  if(readImg_){
    for(int x=0;x<readImgW_;++x){
      for(int y=0;y<readImgH_;++y)
	delete[] readImg_[x][y];
      delete[] readImg_[x];
    }
    delete[] readImg_;
    readImg_ = 0;
  }
}

// ////////////////////////////////////////////////////////////////////////
// //default(0)-is all black,
// void PixelHistoPicGen::initImgBuffer(int sourceKey){
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

// ////////////////////////////////////////////////////////////////////////
// //default(0)-is all black,
// void PixelHistoPicGen::initBImgBuffer(int sourceKey){
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

// ////////////////////////////////////////////////////////////////////////
// void PixelHistoPicGen::initImgBuffer(int r,int g,int b){

// 	for(int x=0;x<IMG_WIDTH;++x)
// 		for(int y=0;y<IMG_HEIGHT;++y){
// 		  img_[x][y][0] = r;
// 		  img_[x][y][1] = g;
// 		  img_[x][y][2] = b;
// 		}
	
// }

// ////////////////////////////////////////////////////////////////////////
// void PixelHistoPicGen::initBImgBuffer(int r,int g,int b){

// 	for(int x=0;x<BIMG_WIDTH;++x)
// 		for(int y=0;y<BIMG_HEIGHT;++y){
// 		  bimg_[x][y][0] = r;
// 		  bimg_[x][y][1] = g;
// 		  bimg_[x][y][2] = b;
// 		}
	
// }

////////////////////////////////////////////////////////////////////////
//read bmp file to readImg_ buffer
void PixelHistoPicGen::readBmpToReadImg(const string& filename){

//   string mthn = "[PicGen::readBMPToReadImg()]\t";

  ifstream file(filename.c_str(),ifstream::binary);
  if(!file.is_open())
    return;

//   cout << mthn << "Has file." << endl;

  //BMP Header
  char buffer[64];
  file.read(buffer,2); //"BM"
  unsigned int size;
  file.read((char *)(&size),4);
//   cout << mthn << "size: " << size << endl;
  file.read(buffer,4);
  unsigned int data_offset;
  file.read((char *)(&data_offset),4);
//   cout << mthn << "doff: " << data_offset << endl;
  unsigned int header_size;
  file.read((char *)(&header_size),4);
//   cout << mthn << "hsize: " << header_size << endl;
  unsigned int w,h;
  file.read((char *)(&w),4);
  file.read((char *)(&h),4);
//   cout << mthn << w << " " << h << endl;
  file.read(buffer,2);
  unsigned int depth = 0;
  file.read((char *)(&depth),2);
//   cout << mthn << depth << endl;
  if(depth != 24 && depth != 32){
    file.close();
    return;
  }
//   cout << mthn << "Depth correct." << endl;
  unsigned int compr_method;
  file.read((char *)(&compr_method),4);
//   cout << mthn << "method: " << compr_method << endl;

  //BMP Data
  file.seekg(data_offset,ios_base::beg); //set file position to start of data

  //dealloc old
  if(readImg_){
    for(int x=0;x<readImgW_;++x){
      for(int y=0;y<readImgH_;++y)
	delete[] readImg_[x][y];
      delete[] readImg_[x];
    }
    delete[] readImg_;
    readImg_ = 0;
  }

  //allocate
  readImgW_ = w;
  readImgH_ = h;
  readImg_ = new unsigned char**[readImgW_];
  for(int x=0;x<readImgW_;++x){
    readImg_[x] = new unsigned char*[readImgH_];
      for(int y=0;y<readImgH_;++y)
				readImg_[x][y] = new unsigned char[3];
  }

  //read data
  for(int y=readImgH_-1;y>=0;--y)
    for(int x=0;x<readImgW_;++x){
      for(int i=2;i>=0;--i)
				file.read((char *)(&(readImg_[x][y][i])),1);
      if(depth == 32) //skip alpha byte
				file.seekg(1,ios_base::cur); //skip one byte
    }

  file.close();
}

// ////////////////////////////////////////////////////////////////////////
// //transfer readImg_ buffer to img_ buffer
// void PixelHistoPicGen::transferReadImgToImg(){
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

// ////////////////////////////////////////////////////////////////////////
// //transfer readImg_ buffer to bimg_ buffer
// void PixelHistoPicGen::transferReadImgToBImg(){
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
//write img buffer to bmp file
void PixelHistoPicGen::writeImgToBmp(string filename){
  
// BMP Header				Stores general information about the BMP file.
// Bitmap Information (DIB header)	Stores detailed information about the bitmap image.
// Color Palette			Stores the definition of the colors being used for indexed color bitmaps.
// Bitmap Data				Stores the actual image, pixel by pixel.

  ofstream file(filename.c_str(),ofstream::binary);
  if(!file.is_open())
    return;

  unsigned int bmpTemp;
	bmpTemp=IMG_FILE_SIZE;
	
  //BMP Header
  file << char(0x42) << char(0x4d); //"BM"
  for(int i=0;i<4;++i){ //file size in little-endian
    file << (unsigned char)(((char *)(&bmpTemp))[i]);  
  }
  file << char(0x00)<< char(0x00)<< char(0x00)<< char(0x00);//0x00000000; //reserved bytes
  file << char(0x36)<< char(0x00)<< char(0x00)<< char(0x00);//0x36000000; //offset to data
  file << char(0x28)<< char(0x00)<< char(0x00)<< char(0x00);//0x28000000; //size of DIB header
  for(int i=0,bmpTemp=IMG_WIDTH;i<4;++i){ //img pixel width in little-endian
    file << (unsigned char)(((char *)(&bmpTemp))[i]);
  }
  for(int i=0,bmpTemp=IMG_HEIGHT;i<4;++i){ //img pixel height in little-endian
    file << (unsigned char)(((char *)(&bmpTemp))[i]);
  }
  file <<char(0x01)<<char(0x00)<<char(0x18)<<char(0x00)<<\
    char(0x00)<<char(0x00)<<char(0x00)<<char(0x00); //details and 24-bit depth
  for(int i=0,bmpTemp=IMG_RAW_SIZE;i<4;++i){ //file raw data size in little-endian
    file <<(unsigned char)(((char *)(&bmpTemp))[i]);    
  }
								
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00)<<char(0x00)<<\
    char(0x00)<<char(0x00)<<char(0x00)<<char(0x00)<<			\
    char(0x00)<<char(0x00)<<char(0x00)<<char(0x00)<<			\
    char(0x00)<<char(0x00)<<char(0x00); //finish header details
  
  //BMP Data
  
  for(int y=IMG_HEIGHT-1;y>=0;--y)
    for(int x=0;x<IMG_WIDTH;++x)
      for(int i=2;i>=0;--i){
				file << img_[x][y][i];
      }

  file.close();
  
}

// ////////////////////////////////////////////////////////////////////////
// //write bimg buffer to bmp file
// void PixelHistoPicGen::writeBImgToBmp(string filename){
  
//   ofstream file(filename.c_str(),ofstream::binary);
//   if(!file.is_open())
//     return;

//   unsigned int bmpTemp;
// 	bmpTemp=BIMG_FILE_SIZE;
	
//   //BMP Header
//   file << char(0x42) << char(0x4d); //"BM"
//   for(int i=0;i<4;++i){ //file size in little-endian
//     file << (unsigned char)(((char *)(&bmpTemp))[i]);  
//   }
//   file << char(0x00)<< char(0x00)<< char(0x00)<< char(0x00);//0x00000000; //reserved bytes
//   file << char(0x36)<< char(0x00)<< char(0x00)<< char(0x00);//0x36000000; //offset to data
//   file << char(0x28)<< char(0x00)<< char(0x00)<< char(0x00);//0x28000000; //size of DIB header
//   for(int i=0,bmpTemp=BIMG_WIDTH;i<4;++i){ //img pixel width in little-endian
//     file << (unsigned char)(((char *)(&bmpTemp))[i]);
//   }
//   for(int i=0,bmpTemp=BIMG_HEIGHT;i<4;++i){ //img pixel height in little-endian
//     file << (unsigned char)(((char *)(&bmpTemp))[i]);
//   }
//   file <<char(0x01)<<char(0x00)<<char(0x18)<<char(0x00)<<
//     char(0x00)<<char(0x00)<<char(0x00)<<char(0x00); //details and 24-bit depth
//   for(int i=0,bmpTemp=BIMG_RAW_SIZE;i<4;++i){ //file raw data size in little-endian
//     file <<(unsigned char)(((char *)(&bmpTemp))[i]);    
//   }
								
//   file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00)<<char(0x00)<<
//     char(0x00)<<char(0x00)<<char(0x00)<<char(0x00)<<			
//     char(0x00)<<char(0x00)<<char(0x00)<<char(0x00)<<			
//     char(0x00)<<char(0x00)<<char(0x00); //finish header details
  
//   //BMP Data
  
//   for(int y=BIMG_HEIGHT-1;y>=0;--y)
//     for(int x=0;x<BIMG_WIDTH;++x)
//       for(int i=2;i>=0;--i){
// 				file << bimg_[x][y][i];
//       }

//   file.close();
  
// }

////////////////////////////////////////////////////////////////////////
void PixelHistoPicGen::convertBmp  (const std::string& fileBMP, const std::string& convertFile){
	string convertCmd = "convert " + fileBMP + " " + convertFile;
	system(convertCmd.c_str());
}

// ////////////////////////////////////////////////////////////////////////
// //draw specified disc to img_ -- 8 of them (0-7: even are 4 plaquette panels and odd are 3)
// void PixelHistoPicGen::drawDiscToImg(int disc, bool isClicked){

//   //string mthn = "[PixelHistoPicGen::drawDetectorToImg()]\t";
//   resetFPixGood(disc);
// 	drawPanelToImg(disc/2,disc%2,DET_DISCL_X,DET_DISCL_Y, isClicked);
	
//   //drawPanelToImg(disc,0,DET_DISCR_X,DET_DISCR_Y);	//4 Plaquette Panel
// 	//drawPanelToImg(disc,1,DET_DISCL_X,DET_DISCL_Y);	//3 Plaquette Panel

// }

// ////////////////////////////////////////////////////////////////////////
// //{x,y} is the center coord
// void PixelHistoPicGen::drawPanelToImg(int disc, int panel, int x, int y, bool isClicked){
     
//   float up[2] = {0.0f,-1.0f}; 
 
//   float rad_delta = PI/24.0f; //start 7.5deg offset from center
//   float deg = 7.5;

//   transform(up[0], up[1], cos(rad_delta), -sin(rad_delta), sin(rad_delta), cos(rad_delta));  //transform up vector
  
//   rad_delta = PI/12.0f;
//   float deg_delta = 15.0f;  
//   int radius = DET_DISC_RADIUS;
//   float px,py;

// 	int halfDisc,blade;
// 	halfDisc = 0;
	
// 	int clicki = 0;
// 	int roci = 0;
	
//   for(int i=0;i<24;++i){
	
// 		if(i >= 12){
// 			if(!halfDisc){
// 				halfDisc = 1;
// 			}
// 			blade = 11 - (i-12);								
// 		}
// 		else{
// 			blade = i;
// 		}
		
//     px = x + up[0]*radius;
//     py = y + up[1]*radius;

//     drawBladeToImg(disc,panel,halfDisc,blade,(int)px,(int)py,deg,roci,clicki,isClicked);
    
//     deg += deg_delta;
//     transform(up[0], up[1], cos(rad_delta), -sin(rad_delta), sin(rad_delta), cos(rad_delta));  //transform up vector
//   }
// }

// ////////////////////////////////////////////////////////////////////////
// //{x,y} is the center of the top if bottom is smallest plaquette
// // deg is degrees of righthand rotation around +z-axis
// void PixelHistoPicGen::drawBladeToImg(int disc,int panel,int halfDisc,int blade,
// 														 int x,int y,float deg,int &roci,int &clicki,bool isClicked){
//   string mthn = "[PixelHistoPicGen::drawBladeToImg()]\t";
//   float up[2] = {0.0f,1.0f}; 
//   float rad = deg*PI/180.0f;
//   transform(up[0], up[1], cos(rad), -sin(rad), sin(rad), cos(rad));  //transform up vector
//   float rt[2] = {up[1],-up[0]}; //get rt from up vector
  
//   int dimensions[] = {5,2,4,2,3,2,0,0};
//   if(panel==0){
//     dimensions[1] = 1;
//     dimensions[6] = 2;
//     dimensions[7] = 1;
//   }

//   float tmpx,tmpy,tmprx,tmpry;
//   int sz = DET_ROC_SIZE;
//   int ROC_off = DET_ROC_OFFSET;
//   int plaq_off = DET_PLAQ_OFFSET;
  
//   float rx = x - rt[0]*(2.5f*sz+2.0f*ROC_off); //center {x,y}
//   float ry = y - rt[1]*(2.5f*sz+2.0f*ROC_off);

// 	int rd,gn,bl;
	
// 	int roc = panel ? 23:20; //roc numbering is 0 in bottom left and then snakes rows
//   int tmproc;
// 	for(int layer=0;layer<4;++layer){
//     tmprx = rx;
//     tmpry = ry;

// 		tmproc = roc;
//     for(int j=0;j<dimensions[layer*2+1];++j){
//       tmpx = rx;
//       tmpy = ry;	
			
// 			if(dimensions[layer*2+1] == 1){
// 				roc -= dimensions[layer*2]-1;
// 			}
				
//       for(int i=0;i<dimensions[layer*2];++i){
			
// 				rd = fpix_[disc][panel][halfDisc][blade][roc][0];
//   			gn = fpix_[disc][panel][halfDisc][blade][roc][1];
//   			bl = fpix_[disc][panel][halfDisc][blade][roc][2];
// 				if( rd!=COLOR_INIT_R || gn!=COLOR_INIT_G || bl!=COLOR_INIT_B){
// 					fpixOn_[disc*2+panel] = 1;
// 					if( rd!=COLOR_GOOD_R || gn!=COLOR_GOOD_G || bl!=COLOR_GOOD_B){
// 					  if( rd==COLOR_BAD_R && gn==COLOR_BAD_G && bl==COLOR_BAD_B){
// //							cout << mthn << "Red-" << rd << ":" << gn << ":" << bl << endl;
// 							fpixGood_[disc*2+panel] = -1;
// 						}
// 						else{
// //							cout << mthn << "All-" << rd << ":" << gn << ":" << bl << endl;
// 							fpixGood_[disc*2+panel] = -2;
// 						}					
// 					}
// 				}
	
// 				if(isClicked && clickMask_ != 0 && clickMask_[clicki] == roci){
// 					rd -= CLICK_MASK_R;
// 					gn -= CLICK_MASK_G;
// 					bl -= CLICK_MASK_B;
// 					rd = rd<0?0:rd;
// 					gn = gn<0?0:gn;
// 					bl = bl<0?0:bl;
// 					++clicki;
// 				}
// 				++roci;
					
// 				drawFillRectAng((int)rx,(int)ry,sz,sz,
// 					rd,gn,bl,deg);
					
// 				if(i+1 < dimensions[layer*2]){
// 					if(dimensions[layer*2+1] == 1 || j == 1){
// 						++roc;
// 					}
// 					else{
// 						--roc;
// 					}
// 				}
				
// 				rx += rt[0]*(sz+ROC_off);
// 				ry += rt[1]*(sz+ROC_off);
//       }
			
// 			if(dimensions[layer*2+1] == 2){
// 				roc -= dimensions[layer*2];
// 			}
			
//       rx = tmpx + up[0]*(sz+ROC_off);
//       ry = tmpy + up[1]*(sz+ROC_off);
//     }
// 		roc = tmproc - dimensions[layer*2+1]*dimensions[layer*2];
  
//     rx = tmprx + up[0]*(dimensions[layer*2+1]*(sz+ROC_off)+plaq_off) + rt[0]*0.5f*(sz+ROC_off);
//     ry = tmpry + up[1]*(dimensions[layer*2+1]*(sz+ROC_off)+plaq_off) + rt[1]*0.5f*(sz+ROC_off);
//   }
// }

////////////////////////////////////////////////////////////////////////
void PixelHistoPicGen::drawFillRectAng(int x,int y,int w,int h,
										  int r, int g, int b, float deg){
  
  float rad = deg*PI/180.0f;
  drawFillRect(x,y,w,h,r,g,b, cos(rad), -sin(rad), sin(rad), cos(rad)); 

}

////////////////////////////////////////////////////////////////////////
//draws rect to img buffer. {x,y} is lower left corner. d is degrees of rotation around z-axis.
void PixelHistoPicGen::drawFillRect(int x,int y,int w,int h, 
										  int r, int g, int b, float m1, float m2, float m3, float m4){
  
  float up[2] = {0.0f,1.0f};  

  transform(up[0], up[1], m1, m2, m3, m4);  //transform up vector

  float rt[2] = {up[1],-up[0]}; //get rt from up vector
  
  
  float px = (float)x;
  float py = (float)y; 
  float tpx,tpy;
  
  for(int i=0;i<w*2;++i){
    tpx = px;
    tpy = py;
    for(float j=0;j<h*2;++j){
      setImgPixel((int)px,(int)py,r,g,b);     
      px += up[0]/2.0f;
      py += up[1]/2.0f;
    }
    px = tpx + rt[0]/2.0f;
    py = tpy + rt[1]/2.0f;
  }

}

////////////////////////////////////////////////////////////////////////
void PixelHistoPicGen::setImgPixel(int x, int y, int r, int g, int b){
  img_[x][y][0]=r;
  img_[x][y][1]=g;
  img_[x][y][2]=b;
}

// ////////////////////////////////////////////////////////////////////////
// void PixelHistoPicGen::fillFPixColors(){
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

// ////////////////////////////////////////////////////////////////////////
// string PixelHistoPicGen::getFPixStandardName(int disc, int panel,
// 		int halfDisc, int blade, int roc){

// 	//FPix B(p,m)(I,O) D(1-3) BLD(1-12) PNL(1,2) RNG(1-(3,4)) ROC(0-(1,4,5,7,9))
	
// 	string stdName = "FPix_B";
// 	char intString[3];
	
// 	stdName += disc < 2 			? "m":"p";									 //minus or plus
// 	stdName += halfDisc == 0  ? "I":"O";									 //inner or outter
// 	stdName += "_D";
// 	sprintf(intString,"%d",disc < 2 		? 2-disc:disc-1);
// 	stdName += intString;														  		 //disc 1-2
// 	stdName += "_BLD";
// 	sprintf(intString,"%d",blade+1);
// 	stdName += intString; 																 //blade 1-12
// 	stdName += "_PNL";
// 	sprintf(intString,"%d",panel+1);
// 	stdName += intString; 																 //panel 1-2
// 	stdName += "_RNG";
// 	sprintf(intString,"%d",panel == 0 		? (
// 																roc < 2 ? 1 :
// 																roc < 8 ? 2 :
// 																roc < 16? 3 : 4
// 															):(
// 																roc < 6 ? 1 :
// 																roc < 14? 2 :	3
// 															));	
// 	stdName += intString;																	 //plaquette 1-3,4
// 	stdName += "_ROC";
// 	sprintf(intString,"%d",panel == 0 		? (
// 																roc < 2 ? roc 	: 
// 																roc < 8 ? roc-2 :
// 																roc < 16? roc-8	: roc-16
// 															):(
// 																roc < 6 ? roc 	: 
// 																roc < 14? roc-6 :	roc-14														
// 															));
// 	stdName += intString;												  					//roc 0-1,4,5,7,9
	
	
	
// 	return stdName;	
// }

// ////////////////////////////////////////////////////////////////////////
// void PixelHistoPicGen::getFPixIndices(string stdName, int &disc, int &panel,
// 		int &halfDisc, int &blade, int &roc){

// 	string mthn = "[PixelHistoViewer::getFPixIndices()]\t";
	
// 	//fpix_[disc][panel][halfDisc][blade][roc][rgb];
	
// 	if(stdName.length() < 30 || stdName.length() > 32){
// 		cout << mthn << "Invalid Length." << endl;
// 		disc = -1; //indicate error
// 		return;
// 	}
	
// 	char intString[3];
// 		//disc -------------------------------------
// 	sprintf(intString,"%c",stdName[10]);
// 	int d = atoi(intString); 
// 	disc = stdName[6] == 'm' ? 2-d:d+1;
	
// 		//panel -------------------------------------
// 	int pnlLoc = stdName.find("_PNL");
// 	if(pnlLoc < 0){
// 		cout << mthn << "Invalid Panel." << endl;
// 		disc = -1;
// 		return;
// 	}
// 	pnlLoc += 4;
// 	sprintf(intString,"%c",stdName[pnlLoc]);
// 	panel = atoi(intString) - 1;

// 		//halfDisc ------------------------------------- 
// 	halfDisc = stdName[7] == 'I' ? 0:1;
	
// 		//blade -------------------------------------
// 	int bldLoc = stdName.find("_BLD");
// 	if(bldLoc < 0){
// 		cout << mthn << "Invalid Blade." << endl;
// 		disc = -1;
// 		return;
// 	}
// 	bldLoc += 4;
// 	sprintf(intString,"%c",stdName[bldLoc]);
// 	if(stdName[bldLoc+1] != '_'){
// 		sprintf(intString,"%s%c",intString,stdName[bldLoc+1]);
// 	}	
// 	blade = atoi(intString) - 1;
	
// 		//roc -------------------------------------
// 	int rngLoc = stdName.find("_RNG");
// 	if(rngLoc < 0){
// 		cout << mthn << "Invalid Plaquette." << endl;
// 		disc = -1;
// 		return;
// 	}
// 	rngLoc += 4;
// 	sprintf(intString,"%c",stdName[rngLoc]);
// 	int ring = atoi(intString)-1; 
	
// 	int rocLoc = stdName.find("_ROC");
// 	if(rocLoc < 0){
// 		cout << mthn << "Invalid ROC." << endl;
// 		disc = -1;
// 		return;
// 	}
// 	rocLoc += 4;
// 	sprintf(intString,"%c",stdName[rocLoc]);
// 	int rocLabel = atoi(intString); 
	
// 	if(panel == 0){ //4 ring panel
// 		int rocsPerPlaq[] = {0,2,8,16};
// 		roc = rocsPerPlaq[ring] + rocLabel;
// 	}
// 	else{ //3 ring panel
// 		int rocsPerPlaq[] = {0,6,14};
// 		roc = rocsPerPlaq[ring] + rocLabel;
// 	}

// }

// ////////////////////////////////////////////////////////////////////////
// string PixelHistoPicGen::getBPixStandardName(int layer,int pm,int row,int col){
// 	string mthn = "[PixelHistoViewer::getBPixStandardName()]\t";
// 	//BPix B(p,m)(I,O) SEC(1-8) LYR(1-3) LDR(1H,2F-(N-1)F,NH) MOD(1-4) ROC(0-(7,15))
	
// 	string stdName = "BPix_B";
// 	char intString[10];
	
// 	int lyrRows[] = {BPIX_LYR1_ROWS,BPIX_LYR2_ROWS,BPIX_LYR3_ROWS};
// 	int endSectorRows[] = {3,3,5};
// 	int otherSectorRows[] = {2,4,6};
	
// 	if(row >= lyrRows[layer] || col >= BPIX_LYR_COLS){
// 		cout << mthn << "Invalid Row and Col: " << row << " " << col << endl;
// 		return "";	
// 	}
	
// 	stdName += pm == 0 ? "m":"p"; 													//minus or plus
	
// 	int moduloCmp = 0;

// 	if(pm == 0){
// 		col = BPIX_LYR_COLS - 1 - col;
// 		moduloCmp = 1;
// 	}

// 																													//find inner/outer
// 	char inOut;
// 	if(row < lyrRows[layer]/2){
// 		inOut = 'I';
// 	}
// 	else{
// 		inOut = 'O';	
// 		row = lyrRows[layer] - 1 - row;
// 	}
// 	sprintf(intString,"%c",inOut);
// 	stdName += intString;
// 																													//find sector
// 	int sector;
// 	if(row < endSectorRows[layer]){ //first sector
// 		sector = 1; 
// 	}
// 	else{
// 		row -= endSectorRows[layer];
// 		sector = 2 + row/otherSectorRows[layer];
// 		if(sector > 8)
// 			sector = 8;
			
// 		if(layer == 2 && sector >= 4){
// 			row -= otherSectorRows[layer]*2;
// 			if(row < 8){ //in sector 4 or 5
// 				sector = 4 + row/4;
// 				row = row%4;
// 			}
// 			else{
// 				sector = 6 + (row-8)/otherSectorRows[layer];
// 				row = (row-8)%otherSectorRows[layer];			
// 			}
// 		}
// 		else{
// 			row -= otherSectorRows[layer]*(sector-2);		
// 		}
// 	}
// 	sprintf(intString,"_SEC%d",sector);
// 	stdName += intString;		
	
// 	sprintf(intString,"_LYR%d",layer+1); 												//layer
// 	stdName += intString;		
	
// 	//row now is the row withing the sector

// 	int doubleRow = row/2;
	
// 																														//find ladder
// 	int ladder;
// 	char halfFull;
// 	if(sector == 1){
// 		if(row == 0){
// 			ladder = 1;
// 			halfFull = 'H';
// 		}
// 		else{
// 			halfFull = 'F';
// 			ladder = 2 + (row-1)/2;		
// 		}
// 	}
// 	else if(layer == 2 && sector >= 4){
// 		if(sector <= 5){
// 			halfFull = 'F';
// 			int startLadder = 10;
// 			ladder = startLadder + (sector-4)*2 + doubleRow;
// 		}
// 		else{
// 			halfFull = 'F';
// 			int startLadder = 14;
// 			ladder = startLadder +
// 				(sector-6)*otherSectorRows[layer]/2 + doubleRow;
// 			if(ladder == 22){
// 				halfFull = 'H';
// 			}
// 		}
// 	}
// 	else if(sector == 8){

// 		ladder = endSectorRows[layer]/2 + 1 + (lyrRows[layer]/2 - endSectorRows[layer])/2 + doubleRow;
	
// 		if(row == endSectorRows[layer] - 1){
// 			halfFull = 'H';
// 		}
// 		else{
// 			halfFull = 'F';
// 		}
// 	}	
// 	else{
// 		halfFull = 'F';
// 		ladder = endSectorRows[layer]/2 + 2 + (sector-2)*otherSectorRows[layer]/2 + doubleRow;
// 	}
// 	sprintf(intString,"_LDR%d%c",ladder,halfFull);
// 	stdName += intString;		
	
// 																																//find module
// 	int module = col/8 + 1;
// 	sprintf(intString,"_MOD%d",module);
// 	stdName += intString;		
		
// 																																//find ROC
// 	int roc;
// 	if( (inOut == 'I' && sector == 1 && row != 0 && row%2 == moduloCmp) ||
// 			(inOut == 'I' && sector == 8 &&
// 				row != endSectorRows[layer] - 1 && row%2 == 1 - moduloCmp) ||
// 			(inOut == 'I' && sector != 8 &&
// 				sector != 1 && row%2 == 1 - moduloCmp) ||
// 			(inOut == 'O' && sector == 1 && row != 0 && row%2 == 1 - moduloCmp) ||
// 			(inOut == 'O' && sector == 8 &&
// 				row != endSectorRows[layer] - 1 && row%2 == moduloCmp) ||
// 			(inOut == 'O' && sector != 8 &&
// 				sector != 1 && row%2 == moduloCmp)){
// 		roc = 8 + (col%8);
// 	}
// 	else{
// 		roc = 7 - (col%8);
// 	}
	
// 	sprintf(intString,"_ROC%d",roc);
// 	stdName += intString;		
	
// 	return stdName;
// }
	
// ////////////////////////////////////////////////////////////////////////
// void PixelHistoPicGen::getBPixIndices(string stdName,int &layer,int &pm,int &row,int &col){
// 	string mthn = "[PixelHistoViewer::getBPixIndices()]\t";
	
// 	int lyrRows[] = {BPIX_LYR1_ROWS,BPIX_LYR2_ROWS,BPIX_LYR3_ROWS};
// 	int lastLadder[] = {10,16,22};
	
	
// 	//bpix_[layer]   [p/m] [rows]  				 [cols] 				 [rgb]; 
// 	//BPix B(p,m)(I,O) SEC(1-8) LYR(1-3) LDR(1H,2F-(N-1)F,NH) MOD(1-4) ROC(0-(7,15))
	
// 	if(stdName.length() < 34 || stdName.length() > 37){
// 		cout << mthn << "Invalid Length: " <<  stdName.length() << endl;
// 		layer = -1; //indicate error
// 		return;
// 	}
	
// 	char intString[3];
	
// 		//layer -------------------------------------
// 	int lyrLoc = stdName.find("_LYR");
// 	if(lyrLoc < 0){
// 		cout << mthn << "Invalid Layer." << endl;
// 		layer = -1;
// 		return;
// 	}
// 	lyrLoc += 4;
// 	sprintf(intString,"%c",stdName[lyrLoc]);
// 	layer = atoi(intString) - 1;
	
// 		//pm -------------------------------------
// 	pm = stdName[6] == 'm' ? 0:1;
	
// 		//GET PARAMETERS FOR ROW AND COL -------------------------------------
// 	char inOut = stdName[7];								//get inout
// 	int sector;
// 	int secLoc = stdName.find("_SEC");
// 	if(secLoc < 0){
// 		cout << mthn << "Invalid Layer." << endl;
// 		layer = -1;
// 		return;
// 	}
// 	secLoc += 4;
// 	sprintf(intString,"%c",stdName[secLoc]);
// 	sector = atoi(intString); 							//get sector
	
// 	int ladder;
// 	int ldrLoc = stdName.find("_LDR");
// 	if(ldrLoc < 0){
// 		cout << mthn << "Invalid Layer." << endl;
// 		layer = -1;
// 		return;
// 	}
// 	ldrLoc += 4;
// 	sprintf(intString,"%c",stdName[ldrLoc]);
// 	if(stdName[ldrLoc+1] != 'H' && stdName[ldrLoc+1] != 'F'){
// 		sprintf(intString,"%s%c",intString,stdName[ldrLoc+1]);
// 		++ldrLoc;
// 	}	
// 	ladder = atoi(intString); 							//get ladder
// 	int module;
// 	int modLoc = stdName.find("_MOD");
// 	if(modLoc < 0){
// 		cout << mthn << "Invalid Layer." << endl;
// 		layer = -1;
// 		return;
// 	}
// 	modLoc += 4;
// 	sprintf(intString,"%c",stdName[modLoc]);
// 	module = atoi(intString); 							//get module
// 	int roc;
// 	int rocLoc = stdName.find("_ROC");
// 	if(rocLoc < 0){
// 		cout << mthn << "Invalid Layer." << endl;
// 		layer = -1;
// 		return;
// 	}
// 	rocLoc += 4;
// 	sprintf(intString,"%c",stdName[rocLoc]);
// 	if((unsigned int)rocLoc+1 < stdName.length() && stdName[rocLoc+1] != '\0'){
// 		sprintf(intString,"%s%c",intString,stdName[rocLoc+1]);
// 	}	
// 	roc = atoi(intString); 									//get roc
	
// 		//row -------------------------------------
// 	if(inOut == 'I'){
// 		if(ladder == 1){
// 			row = 0;
// 		}
// 		else{
// 			row = 1 + (ladder-2) * 2;
// 		}
// 	}
// 	else{
// 		if(ladder == 1){
// 			row = lyrRows[layer] - 1;
// 		}
// 		else{
// 			row = lyrRows[layer] - 2 - (1 + (ladder-2) * 2);
// 			if(ladder == lastLadder[layer])
// 				++row;
// 		}
// 	}
	
// 	if(pm == 0 && ladder != 1 && ladder != lastLadder[layer] && roc < 8 ){
// 		++row;	
// 	}
// 	else if(pm == 1 && ladder != 1 && ladder != lastLadder[layer] && roc >= 8 ){
// 		++row;	
// 	}	
	
// 		//col -------------------------------------
// 	col = pm == 0 ? (4-module)*8 : (module-1)*8;
// 	if(pm == 0){
// 		if(roc < 8)
// 			col += roc;
// 		else
// 			col += 15 - roc;	
// 	}
// 	else{
// 		if(roc < 8)
// 			col += 7 - roc;
// 		else
// 			col += roc - 8;	
// 	}
// }

// ////////////////////////////////////////////////////////////////////////
// void PixelHistoPicGen::setRocColor(string stdName,int rd,int gn,int bl){
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

// ////////////////////////////////////////////////////////////////////////
// void PixelHistoPicGen::setRocColor(string stdName,bool isGood){
// 	if(isGood)
// 		setRocColor(stdName,COLOR_GOOD_R,COLOR_GOOD_G,COLOR_GOOD_B);
// 	else
// 		setRocColor(stdName,COLOR_BAD_R,COLOR_BAD_G,COLOR_BAD_B);	
// }

// ////////////////////////////////////////////////////////////////////////
// void PixelHistoPicGen::fillBPixColors(){
// 	int rows;
// 	for(int l=0;l<3;++l){
// 		rows = l==0?BPIX_LYR1_ROWS:l==1?BPIX_LYR2_ROWS:BPIX_LYR3_ROWS;
// 		for(int pm=0;pm<2;++pm)
// 			for(int r=0;r<rows;++r)
// 				for(int c=0;c<32;++c){
// 					bpix_(l,pm,r,c,0) = COLOR_INIT_R;
// 					bpix_(l,pm,r,c,1) = COLOR_INIT_G;
// 					bpix_(l,pm,r,c,2) = COLOR_INIT_B;
// 				}	
// 	}
// }

// ////////////////////////////////////////////////////////////////////////
// // layerHalf is 0-5 indicating which half layer to draw
// void PixelHistoPicGen::drawLayerToBImg(int layerHalf, bool isClicked){
// 	string mthn = "[PixelHistoPicGen::drawLayerToBImg()]\t";
//   resetBPixGood(layerHalf);
// 	int layer = layerHalf/2;
// 	int rows = layer==0?BPIX_LYR1_ROWS:layer==1?BPIX_LYR2_ROWS:BPIX_LYR3_ROWS;
	
// 	int pm,r,c,x,y,svx,svy;
// 	x = BPIX_LYRM_XOFF;
// 	y = BPIX_LYRM_YOFF;

// 	int roci = 0;
// 	int clicki = 0;
// 	int rd,gn,bl;
// 	for(pm=layerHalf%2;pm<layerHalf%2+1;++pm){ //only do one half of layer 
// 		svy = y;
// 		for(r=0;r<BPIX_LYR3_ROWS;++r){
// 			svx = x;
// 			if(r<rows){//in layer
// 				for(c=0;c<BPIX_LYR_COLS;++c){
				
// 					rd = bpix_(layer,pm,r,c,0);
//   				gn = bpix_(layer,pm,r,c,1);
//   				bl = bpix_(layer,pm,r,c,2);
					
// 					if( rd!=COLOR_INIT_R || gn!=COLOR_INIT_G || bl!=COLOR_INIT_B){
// 						bpixOn_[layer*2+pm] = 1;
// 						if( rd!=COLOR_GOOD_R || gn!=COLOR_GOOD_G || bl!=COLOR_GOOD_B){
// 						  if( rd==COLOR_BAD_R && gn==COLOR_BAD_G && bl==COLOR_BAD_B){
// //								cout << mthn << "Red-" << rd << ":" << gn << ":" << bl << endl;
// 								bpixGood_[layer*2+pm] = -1;
// 							}
// 							else{
// //								cout << mthn << "All-" << rd << ":" << gn << ":" << bl << endl;
// 								bpixGood_[layer*2+pm] = -2;
// 							}					
// 						}
// 					}
// 					if(isClicked && clickMask_ != 0 && clickMask_[clicki] == roci){
// 						rd -= CLICK_MASK_R;
// 						gn -= CLICK_MASK_G;
// 						bl -= CLICK_MASK_B;
// 						rd = rd<0?0:rd;
// 						gn = gn<0?0:gn;
// 						bl = bl<0?0:bl;				
// 						++clicki;		
// 					}
					
// 					drawBPixRoc(x,y,rd,gn,bl);
				
// 					x+=BPIX_ROC_WIDTH+BPIX_COL_OFFSET;
// 					++roci;
// 				}
// 			}
// 			else{//out of layer... possibly use to erase img buffer artifacts
	
// 			}
// 			x = svx;
// 			y+=BPIX_ROC_HEIGHT+BPIX_ROW_OFFSET;
// 		}
// 		//x = BPIX_LYRP_XOFF;
// 		//y = BPIX_LYRP_YOFF;
// 	}

// }


// ////////////////////////////////////////////////////////////////////////
// void PixelHistoPicGen::drawBPixRoc(int x,int y,int r,int g,int b){
// 	int i,j;
// 	for(i=x;i<x+BPIX_ROC_WIDTH;++i)
// 		for(j=y;j<y+BPIX_ROC_HEIGHT;++j){
// 			bimg_[i][j][0] = r;
// 			bimg_[i][j][1] = g;
// 			bimg_[i][j][2] = b;
// 		}
// }


void recurseForBg(unsigned char ***d,int r,int c,int rm, int cm){
	if(r == rm || c == cm || r<0 || c<0)
		return;
		
	if(d[r][c][0] == 255 && d[r][c][1] == 0 && d[r][c][2] == 0) //already found
		return;
		
	if(d[r][c][0] > 100){ //white so mark & check for neighbors
		d[r][c][0] = 255;
		d[r][c][1] = 0;
		d[r][c][2] = 0;
		recurseForBg(d,r+1,c,rm,cm); //right
		recurseForBg(d,r-1,c,rm,cm); //left
		recurseForBg(d,r,c-1,rm,cm); //up
		recurseForBg(d,r,c+1,rm,cm); //dn
	}
} 

#include <sys/stat.h>  // for mkdir
void PixelHistoPicGen::generateTurtle(const std::string& filepath)
{
	std::string tmpPath = filepath + "generated/tmp.bmp";
	int offSetH;
	
	if(firstTurtle)
	{ //create first turtle
		firstTurtle = false;

		// attempt to make directory structure (just in case)
		mkdir((filepath + "generated").c_str(), 0755);
	 
		readBmpToReadImg(filepath + "turtle.bmp");
	
		if(readImgW_ == 0)
			return;
	
		offSetH = (TUR_IMG_HEIGHT - readImgH_)/2;
		
		clearTurtleBuffer(255,255,255,255);
		
		recurseForBg(readImg_,0,0,readImgW_,readImgH_);
		recurseForBg(readImg_,150,readImgH_-1,readImgW_,readImgH_);
	
		// std::cout << "readImgW_ " << readImgW_ << std::endl;
		// std::cout << "readImgH_ " << readImgH_ << std::endl;
		for(int i=0;i<readImgW_;++i)
			for(int j=0;j<readImgH_;++j)
			{
				if( readImg_[i][j][0] < 60 && 
						readImg_[i][j][1] > 130)
				{
					turtleImg_[i][offSetH+j][0] = 0;
					turtleImg_[i][offSetH+j][1] = 0;
					turtleImg_[i][offSetH+j][2] = 255;
//					cout << (int)readImg_[i][j][0] << " " << (int)readImg_[i][j][1] << " " <<	(int)readImg_[i][j][2] <<endl;
  				}
				else
					for(int k=0;k<3;++k)
						turtleImg_[i][offSetH+j][k] = readImg_[i][j][k];
	
				
			 	if(readImg_[i][j][0] == 255 &&
  					readImg_[i][j][1] == 0 &&
  					readImg_[i][j][2] == 0)
  				turtleImg_[i][offSetH+j][3] = 0; //invisible
  			else
  				turtleImg_[i][offSetH+j][3] = 255;
			}
	
		writeTurtleToBmp((filepath + "generated/turtleBase.bmp").c_str());

	}
	
	//change color
	readBmpToReadImg(filepath + "generated/turtleBase.bmp");
	offSetH = (TUR_IMG_HEIGHT - readImgH_)/2;
	
	int rd = clock()%256;
	int gn = (clock()/3)%256;
	int bl = (clock()*3)%256;	
	
	for(int i=0;i<readImgW_;++i)
		for(int j=0;j<readImgH_;++j)
		 	if(readImg_[i][j][0] == 0 &&
					readImg_[i][j][1] == 0 &&
					readImg_[i][j][2] == 255){
				turtleImg_[i][offSetH+j][0] = rd;
				turtleImg_[i][offSetH+j][1] = gn;
				turtleImg_[i][offSetH+j][2] = bl;
			}
			
	writeTurtleToBmp(tmpPath.c_str());
	convertBmp(tmpPath, filepath + "generated/turtle.png");
} //end generateTurtle()

// ////////////////////////////////////////////////////////////////////////
// //creates the png's for the different angled ROC highlights for js mouseover
// // and the good/bad boxes
// void PixelHistoPicGen::createAuxImages(){

// 		//create good/bad boxes
// 	char tmpPath[] = "images/generated/tmp.bmp";

// 	clearAuxBuffer(COLOR_GOOD_R,COLOR_GOOD_G,COLOR_GOOD_B,0);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/good.png");

// 	clearAuxBuffer(COLOR_BAD_R,COLOR_BAD_G,COLOR_BAD_B,0);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/bad.png");

// 	clearAuxBuffer(COLOR_INIT_R,COLOR_INIT_G,COLOR_INIT_B,0);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/off.png");

// 	clearAuxBuffer(0,0,0,255);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/invisible.png");

// 	clearAuxBuffer(COLOR_HIGHLIGHT_R,COLOR_HIGHLIGHT_G,COLOR_HIGHLIGHT_B,0);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/rocHighlight.png");

// 		//create alpha-background roc highlights
//   char convertPng[1000];
//   for(int i=0;i<6;++i){ //draw all 6 angles
//     clearAuxBuffer(0,0,0,255);
 
//     drawFillRectAngAux(AUX_IMG_WIDTH/2+1,
//        AUX_IMG_HEIGHT/2+1,
//        WEB_ROC_SIZE*2+8,
//        WEB_ROC_SIZE*2+8,
//        COLOR_HIGHLIGHT_R,COLOR_HIGHLIGHT_G,COLOR_HIGHLIGHT_B,
//        7.5+i*15);
 
//     writeAuxToBmp(tmpPath);
//     sprintf(convertPng,"images/generated/rocHighlight%d.png",i);		
//   	convertBmp(tmpPath,convertPng);
//   }
	
// 		//create summary color keys
// 	//for boolean
// 	for(int x=0;x<IMG_WIDTH;++x)
// 		for(int y=0;y<IMG_HEIGHT;++y)
// 		  setImgPixel(x,y,
// 				x>IMG_WIDTH/2?COLOR_GOOD_R:COLOR_BAD_R,
// 				x>IMG_WIDTH/2?COLOR_GOOD_G:COLOR_BAD_G,
// 				x>IMG_WIDTH/2?COLOR_GOOD_B:COLOR_BAD_B);
				
// 	writeImgToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/summaryColorKeyBoolean.png");
	
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
	
// 	float v;
// 	float sizeOfGrade = 1.0/(numOfColors-1);
// 	int ci;

// 		//blended color key
// 	for(int x=0;x<IMG_WIDTH;++x)
// 		for(int y=0;y<IMG_HEIGHT;++y)
// 		{
			
// 			v = (float)x/IMG_WIDTH;
// 			ci = (int)(v/sizeOfGrade);
// 			v -= ci*sizeOfGrade;
// 			v /= sizeOfGrade; //0-1
// 			setImgPixel(x,y,
// 				(int)(colors[ci][0]*(1-v) + colors[ci+1][0]*v),
// 				(int)(colors[ci][1]*(1-v) + colors[ci+1][1]*v),
// 				(int)(colors[ci][2]*(1-v) + colors[ci+1][2]*v));
// 		}
				
// 	writeImgToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/summaryColorKey.png");
	
// 		//for overflow
// 	clearAuxBuffer(COLOR_HI_R,COLOR_HI_G,COLOR_HI_B,0);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/summaryOverflowKey.png");
// 		//for underflow
// 	clearAuxBuffer(COLOR_LO_R,COLOR_LO_G,COLOR_LO_B,0);
// 	writeAuxToBmp(tmpPath);
// 	convertBmp(tmpPath,"images/generated/summaryUnderflowKey.png");
// }

// void PixelHistoPicGen::createRocAlphaMasks(void)
// {


// }

// ////////////////////////////////////////////////////////////////////////
// //initializes aux buffer to all invisible black pixels
// void PixelHistoPicGen::clearAuxBuffer(int r,int g,int b, int a){ 
//   for(int x=0;x<AUX_IMG_WIDTH;++x)
//     for(int y=0;y<AUX_IMG_HEIGHT;++y){
//       auxImg_[x][y][0] = r;
//       auxImg_[x][y][1] = g;
//       auxImg_[x][y][2] = b;
//       auxImg_[x][y][3] = a;  //255 is invisible, 0 is opaque
//     }
// }


////////////////////////////////////////////////////////////////////////
//initializes aux buffer to all invisible black pixels
void PixelHistoPicGen::clearTurtleBuffer(int r,int g,int b, int a){ 
  for(int x=0;x<TUR_IMG_WIDTH;++x)
    for(int y=0;y<TUR_IMG_HEIGHT;++y){
      turtleImg_[x][y][0] = r;
      turtleImg_[x][y][1] = g;
      turtleImg_[x][y][2] = b;
      turtleImg_[x][y][3] = a;  //255 is invisible, 0 is opaque
    }
}


////////////////////////////////////////////////////////////////////////
void PixelHistoPicGen::writeTurtleToBmp(const char *fn){
  string mthn = "[PicGen::writeTurtleToBmp()]\t";
  
// BMP Header				Stores general information about the BMP file.
// Bitmap Information (DIB header)	Stores detailed information about the bitmap image.
// Color Palette			Stores the definition of the colors being used for indexed color bitmaps.
// Bitmap Data				Stores the actual image, pixel by pixel.

  ofstream file(fn,ofstream::binary);
  if(!file.is_open())
    return;


  unsigned int bmpTemp;
	
  //BMP Header
  file << char(0x42) << char(0x4d);					//"BM"
	bmpTemp = TUR_IMG_FILE_SIZE;
  for(int i=0;i<4;++i)						        //file size in little-endian
    file << (unsigned char)(((char *)(&bmpTemp))[i]);  
  file << char(0x00)<< char(0x00)<< char(0x00)<< char(0x00);		//reserved bytes
  unsigned int dibHeaderSize = TUR_IMG_HEADER_SIZE + 14;
  for(int i=0;i<4;++i)							//DIB header size
    file << (unsigned char)(((char *)(&dibHeaderSize))[i]);
	bmpTemp = TUR_IMG_HEADER_SIZE;	
  for(int i=0;i<4;++i)							//offset to data
    file << (unsigned char)(((char *)(&bmpTemp))[i]); 
	bmpTemp = TUR_IMG_WIDTH; 
  for(int i=0;i<4;++i)							//img pixel width in little-endian
    file << (unsigned char)(((char *)(&bmpTemp))[i]);
	bmpTemp = TUR_IMG_HEIGHT;
  for(int i=0;i<4;++i)							//img pixel height in little-endian
    file << (unsigned char)(((char *)(&bmpTemp))[i]);
  file <<char(0x01)<<char(0x00)<<char(0x20)<<char(0x00);		//color planes and bits per pixel
  file << char(0x03)<<char(0x00)<<char(0x00)<<char(0x00);		//compression method
	bmpTemp = TUR_IMG_RAW_SIZE;
  for(int i=0;i<4;++i)							//file raw data size in little-endian
    file <<(unsigned char)(((char *)(&bmpTemp))[i]);    
  file << char(0xD6)<<char(0x0D)<<char(0x00)<<char(0x00);		//horiz resol in pixels per meter
  file << char(0xD6)<<char(0x0D)<<char(0x00)<<char(0x00);		//vert resol in pixels per meter
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//colors used
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//important colors
  file << char(0x00)<<char(0x00)<<char(0xFF)<<char(0x00);		//red mask
  file << char(0x00)<<char(0xFF)<<char(0x00)<<char(0x00);		//green mask
  file << char(0xFF)<<char(0x00)<<char(0x00)<<char(0x00);		//blue mask
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0xFF);		//alpha mask
  file << char(0x01)<<char(0x00)<<char(0x00)<<char(0x00);		//color space
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//red X
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//red Y
  file << char(0x01)<<char(0x00)<<char(0x00)<<char(0x00);		//red Z
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//green X
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//green Y
  file << char(0x01)<<char(0x00)<<char(0x00)<<char(0x00);		//green Z
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//blue X
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//blue Y
  file << char(0x01)<<char(0x00)<<char(0x00)<<char(0x00);		//blue Z
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//gamma red
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//gamma green
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//gamma blue
  
  //BMP Data

  int bytesToPad = (4-TUR_IMG_WIDTH*4%4)%4; //each row must start on a multiple of 4 byte offset
  for(int y=TUR_IMG_HEIGHT-1;y>=0;--y){
    for(int x=0;x<TUR_IMG_WIDTH;++x){
      for(int i=2;i>=0;--i){
				file << turtleImg_[x][y][i];
      }
      file << turtleImg_[x][y][3]; //alpha byte last
    }
    //pad bytes
    for(int p=0;p<bytesToPad;++p)
      file << char(0x00);
  }

  file.close();
  
}
////////////////////////////////////////////////////////////////////////
void PixelHistoPicGen::writeAuxToBmp(char *fn){
  string mthn = "[PicGen::writeAuxToBmp()]\t";
  
// BMP Header				Stores general information about the BMP file.
// Bitmap Information (DIB header)	Stores detailed information about the bitmap image.
// Color Palette			Stores the definition of the colors being used for indexed color bitmaps.
// Bitmap Data				Stores the actual image, pixel by pixel.

  ofstream file(fn,ofstream::binary);
  if(!file.is_open())
    return;


  unsigned int bmpTemp;
	
  //BMP Header
  file << char(0x42) << char(0x4d);					//"BM"
	bmpTemp = AUX_IMG_FILE_SIZE;
  for(int i=0;i<4;++i)						        //file size in little-endian
    file << (unsigned char)(((char *)(&bmpTemp))[i]);  
  file << char(0x00)<< char(0x00)<< char(0x00)<< char(0x00);		//reserved bytes
  unsigned int dibHeaderSize = AUX_IMG_HEADER_SIZE + 14;
  for(int i=0;i<4;++i)							//DIB header size
    file << (unsigned char)(((char *)(&dibHeaderSize))[i]);
	bmpTemp = AUX_IMG_HEADER_SIZE;	
  for(int i=0;i<4;++i)							//offset to data
    file << (unsigned char)(((char *)(&bmpTemp))[i]); 
	bmpTemp = AUX_IMG_WIDTH; 
  for(int i=0;i<4;++i)							//img pixel width in little-endian
    file << (unsigned char)(((char *)(&bmpTemp))[i]);
	bmpTemp = AUX_IMG_HEIGHT;
  for(int i=0;i<4;++i)							//img pixel height in little-endian
    file << (unsigned char)(((char *)(&bmpTemp))[i]);
  file <<char(0x01)<<char(0x00)<<char(0x20)<<char(0x00);		//color planes and bits per pixel
  file << char(0x03)<<char(0x00)<<char(0x00)<<char(0x00);		//compression method
	bmpTemp = AUX_IMG_RAW_SIZE;
  for(int i=0;i<4;++i)							//file raw data size in little-endian
    file <<(unsigned char)(((char *)(&bmpTemp))[i]);    
  file << char(0xD6)<<char(0x0D)<<char(0x00)<<char(0x00);		//horiz resol in pixels per meter
  file << char(0xD6)<<char(0x0D)<<char(0x00)<<char(0x00);		//vert resol in pixels per meter
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//colors used
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//important colors
  file << char(0x00)<<char(0x00)<<char(0xFF)<<char(0x00);		//red mask
  file << char(0x00)<<char(0xFF)<<char(0x00)<<char(0x00);		//green mask
  file << char(0xFF)<<char(0x00)<<char(0x00)<<char(0x00);		//blue mask
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0xFF);		//alpha mask
  file << char(0x01)<<char(0x00)<<char(0x00)<<char(0x00);		//color space
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//red X
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//red Y
  file << char(0x01)<<char(0x00)<<char(0x00)<<char(0x00);		//red Z
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//green X
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//green Y
  file << char(0x01)<<char(0x00)<<char(0x00)<<char(0x00);		//green Z
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//blue X
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//blue Y
  file << char(0x01)<<char(0x00)<<char(0x00)<<char(0x00);		//blue Z
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//gamma red
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//gamma green
  file << char(0x00)<<char(0x00)<<char(0x00)<<char(0x00);		//gamma blue
  
  //BMP Data

  int bytesToPad = (4-AUX_IMG_WIDTH*4%4)%4; //each row must start on a multiple of 4 byte offset
  for(int y=AUX_IMG_HEIGHT-1;y>=0;--y){
    for(int x=0;x<AUX_IMG_WIDTH;++x){
      for(int i=2;i>=0;--i){
				file << auxImg_[x][y][i];
      }
      file << auxImg_[x][y][3]; //alpha byte last
    }
    //pad bytes
    for(int p=0;p<bytesToPad;++p)
      file << char(0x00);
  }

  file.close();
  
}

////////////////////////////////////////////////////////////////////////
//DIFFERENT THAN drawFillRect... x,y is CENTER!!
//draws rect to aux img buffer. {x,y} is center. m1-4 is rotation matrix around z-axis.
void PixelHistoPicGen::drawFillRectAux(int x,int y,int w,int h, int r, int g, int b, float m1, float m2, float m3, float m4){
  
  float up[2] = {0.0f,1.0f};  

  transform(up[0], up[1], m1, m2, m3, m4);  //transform up vector

  float rt[2] = {up[1],-up[0]}; //get rt from up vector
  
  
  float px = (float)x-up[0]*0.5*w-rt[0]*0.5*w; //subtract to get to bottom left corner
  float py = (float)y-up[1]*0.5*h-rt[1]*0.5*h; 
  float tpx,tpy;
  
  for(int i=0;i<w*2;++i){
    tpx = px;
    tpy = py;
    for(float j=0;j<h*2;++j){
      setAuxPixel((int)px,(int)py,r,g,b);     
      px += up[0]/2.0f;
      py += up[1]/2.0f;
    }
    px = tpx + rt[0]/2.0f;
    py = tpy + rt[1]/2.0f;
  }

}

////////////////////////////////////////////////////////////////////////
//DIFFERENT THAN drawFillRect... x,y is CENTER!!
//draws rect to aux img buffer. {x,y} is center. d is degrees of rotation around z-axis.
void PixelHistoPicGen::drawFillRectAngAux(int x,int y,int w,int h, int r, int g, int b, float deg){
  
  float rad = deg*PI/180.0f;
  drawFillRectAux(x,y,w,h,r,g,b, cos(rad), -sin(rad), sin(rad), cos(rad));

}

////////////////////////////////////////////////////////////////////////
void PixelHistoPicGen::setAuxPixel(int x, int y, int r, int g, int b){
  auxImg_[x][y][0]=r;
  auxImg_[x][y][1]=g;
  auxImg_[x][y][2]=b;
  auxImg_[x][y][3]=0;
}

// ////////////////////////////////////////////////////////////////////////
// void PixelHistoPicGen::createClickMask(string maskString,int size){

// 	if(clickMask_ != 0){
// 		delete[] clickMask_;
// 		clickMask_ = 0;
// 	}
	
// 	clickMask_ = new int[size+1];
// 	clickMask_[size] = -1;
	
// 	char intString[20];
// 	int c = 0;
// 	int i = 0;
	
// 	for(i=0;i<size;++i){
// 		sprintf(intString,"%c",maskString[c]);
// 		++c;
// 		while(maskString[c] != ',' && maskString[c] != '\0'){
// 			sprintf(intString,"%s%c",intString,maskString[c]);
// 			++c;
// 		}
// 		++c; //skip comma
// 		clickMask_[i] = atoi(intString);	
// 	}
// }	

// ////////////////////////////////////////////////////////////////////////
// void PixelHistoPicGen::resetFPixGood(int disk){
// 	fpixGood_[disk]=1;
// 	fpixOn_[disk]=0;
// }

// ////////////////////////////////////////////////////////////////////////
// int PixelHistoPicGen::getFPixGood(int disk){
// 	if(!fpixOn_[disk]){
// 		return 0;
// 	}
// 	return fpixGood_[disk];
// }

// ////////////////////////////////////////////////////////////////////////
// void PixelHistoPicGen::resetBPixGood(int halfLayer){
// 	bpixGood_[halfLayer]=1;
// 	bpixOn_[halfLayer]=0;
// }

// ////////////////////////////////////////////////////////////////////////
// int PixelHistoPicGen::getBPixGood(int halfLayer){
// 	if(!bpixOn_[halfLayer]){
// 		return 0;
// 	}
// 	return bpixGood_[halfLayer];
// }

