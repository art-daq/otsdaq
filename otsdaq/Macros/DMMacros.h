#ifndef __DM_MACROS_H__
#define __DM_MACROS_H__

/*
 *   C++ ASCII output decorator
 *
 *   Copyright (C) 2018 by
 *   Dario Menasce        dario.menasce@cern.ch
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 * ----------------------------------------------------------------------

     This macro decorates a cout statement with auxilary information
     elements such as line number, file name etc...
     The most general form of decoration provides the following elements:

     line #   function-name                    file-name        date        time
user-cout arguments 34    ] [int main(int argc, cha**argv))] [example.cc] [Jun 22 2018]
[10:13:54] Result is 1.2

     The following name-spaced variables control the actual output produced:

     int	 DMM::__span__	          : controls the width of the function/file name field
(number of character)s
     int         DMM::__pthr__	     	  : controls the priority level: only cout
statements declared with an equal or above level with respect to the current value of the
DMM::__pthr__ will be printed out bool	 DMM::__enablePrintouts__ : switches of the
following cout statment (does not produce output bool	 DMM::__long__	          :
shortens the name of the function (removes signature if false) bool	 DMM::__meth__
: enables/disables output of the function-name field bool	 DMM::__file__	     	  :
enables/disables output of the file-name field bool	 DMM::__date__	     	  :
enables/disables output of the date field bool	 DMM::__time__	     	  :
enables/disables output of the time field std::string DMM::__c_meth__	  : determines the
color of the function-name field
     std::string DMM::__c_file__	  : determines the color of the file-name field
     std::string DMM::__c_date__	  : determines the color of the date field
     std::string DMM::__c_time__	  : determines the color of the time field

     To GLOBALLY enable/diable colors, just edit this header file and modify the
     #define __USE_COLORS__ true
     statement below to false

     The list of colors is defined below (variables beginning with AC and ending with _)

     Usage example:

     #include <DMMacros.h>

     int main(int argc, char** argv)
     {

      int    i = 5 ;
      double d = 4.6 ;
      string s = "Just a regular string" ;

      someClassWithAVeryLongName * sc = new someClassWithAVeryLongName() ;
      sc->doSomethingInThisClass() ;

      DMM::__pthr__ = 0 ;
      __PRE__(0,{cout << "A: " << i << " " << ACBCyan_ << d << ACPlain_ << " " << s
                << " --------------------------------------------------------------"
                << endl ;})
      __PRE__(3,{cout << "B: " << i << " " << ACBCyan_ << d << ACPlain_ << " " << s <<
endl ;}) DMM::__enablePrintouts__ = false ;
      __PRE__(0,{cout << "C: " << i << " " << ACBCyan_ << d << ACPlain_ << " " << s <<
endl ;}) DMM::__enablePrintouts__ = true ;
      __PRE__(0,{cout << "D: " << i << " " << ACBCyan_ << d << ACPlain_ << " " << s <<
endl ;}) DMM::__long__ = true ; DMM::__file__ = true ;
      __PRE__(0,{cout << "E: " << i << " " << ACBCyan_ << d << ACPlain_ << " " << s <<
endl ;}) DMM::__c_meth__ = ACBGreen_;
      __PRE__(0,{cout << "F: " << i << " " << ACBCyan_ << d << ACPlain_ << " " << s <<
endl ;}) DMM::__c_meth__ = ACBGreen_ + ACReverse_; DMM::__meth__ = false ; DMM::__file__ =
false ; DMM::__date__ = true ; DMM::__time__ = true  ;
      __PRE__(0,{cout << "G: " << i << " " << ACBCyan_ << d << ACPlain_ << " " << s <<
endl ;}) DMM::__meth__ = false ; DMM::__file__ = true ; DMM::__date__ = false ;
DMM::__time__ = false ;
      __PRE__(0,{cout << "H: " << i << " " << ACBCyan_ << d << ACPlain_ << " " << s <<
endl ;}) DMM::__meth__ = true  ; DMM::__file__ = true ; DMM::__date__ = true  ;
DMM::__time__ = true  ; DMM::__c_file__ = ACBBrown_ + ACReverse_;
      __PRE0__(   cout << "I: " << i << " " << ACBCyan_ << d << ACPlain_ << " " << s <<
endl ; )
     }

     Output (except colors...)

17	] [someClassWithAVeryLongName::so...   ] 1 Ctor
23	] [doSomethingInThisClass              ] 2 do whatever needs to be done
39	] [main                                ] A: 5 4.6 Just a regular string
-------------------------------------------------------------- 40	] [main
] B: 5 4.6 Just a regular string 44	] [main                                ] D: 5 4.6
Just a regular string 46	] [int main(int, char**)               ] [main.cc
] E: 5 4.6 Just a regular string 48	] [int main(int, char**)               ] [main.cc
] F: 5 4.6 Just a regular string 51	] [Jun 22 2018] [13:32:26] G: 5 4.6 Just a regular
string 53	] [main.cc                             ] H: 5 4.6 Just a regular string 56
] [int main(int, char**)               ] [main.cc                             ] [Jun 22
2018] [13:32:26] I: 5 4.6 Just a regular string

      To see color, type: more DMMacros.h

[0;36m[1m17[0m[1;33m	]
[0m[1;33m[[1;35m[1msomeClassWithAVeryLongName::so[0;31m...   [0m[1;33m] [0m1
[0;31m[1mCtor [0;36m[1m23[0m[1;33m	]
[0m[1;33m[[1;35m[1mdoSomethingInThisClass           [0m[0m[0m   [0m[1;33m] [0m2
[1;32m[1mdo whatever needs to be done [0;36m[1m39[0m[1;33m	]
[0m[1;33m[[1;35m[1mmain                             [0m[0m[0m   [0m[1;33m] [0mA:
5 [0;36m[1m4.6[0m Just a regular string
-------------------------------------------------------------- [0;36m[1m40[0m[1;33m	]
[0m[1;33m[[1;35m[1mmain                             [0m[0m[0m   [0m[1;33m] [0mB:
5 [0;36m[1m4.6[0m Just a regular string [0;36m[1m44[0m[1;33m	]
[0m[1;33m[[1;35m[1mmain                             [0m[0m[0m   [0m[1;33m] [0mD:
5 [0;36m[1m4.6[0m Just a regular string [0;36m[1m46[0m[1;33m	]
[0m[1;33m[[1;35m[1mint main(int, char**)            [0m[0m[0m   [0m[1;33m]
[0m[1;33m[[0;36m[1mmain.cc                          [0m[0m[0m   [0m[1;33m] [0mE:
5 [0;36m[1m4.6[0m Just a regular string [0;36m[1m48[0m[1;33m	]
[0m[1;33m[[0;32m[1mint main(int, char**)            [0m[0m[0m   [0m[1;33m]
[0m[1;33m[[0;36m[1mmain.cc                          [0m[0m[0m   [0m[1;33m] [0mF:
5 [0;36m[1m4.6[0m Just a regular string [0;36m[1m51[0m[1;33m	]
[0m[1;33m[[1;36m[1mJun 22 2018[0m[1;33m] [0m[1;33m[[1;34m[1m13:44:32[0m[1;33m]
[0mG: 5 [0;36m[1m4.6[0m Just a regular string [0;36m[1m53[0m[1;33m	]
[0m[1;33m[[0;36m[1mmain.cc                          [0m[0m[0m   [0m[1;33m] [0mH:
5 [0;36m[1m4.6[0m Just a regular string [0;36m[1m56[0m[1;33m	]
[0m[1;33m[[0;32m[1m[7mint main(int, char**)            [0m[0m[0m   [0m[1;33m]
[0m[1;33m[[0;33m[1m[7mmain.cc                          [0m[0m[0m   [0m[1;33m]
[0m[1;33m[[1;36m[1mJun 22 2018[0m[1;33m] [0m[1;33m[[1;34m[1m13:44:32[0m[1;33m]
[0mI: 5 [0;36m[1m4.6[0m Just a regular string
 */

#include <iostream>
#include <sstream>

#define __USE_COLORS__ true

#if __USE_COLORS__ == false

#define ACBlack ""
#define ACBlue ""
#define ACGreen ""
#define ACCyan ""
#define ACRed ""
#define ACPurple ""
#define ACBrown ""
#define ACGray ""
#define ACDarkGray ""
#define ACLightBlue ""
#define ACLightGreen ""
#define ACLightCyan ""
#define ACLightRed ""
#define ACLightPurple ""
#define ACYellow ""
#define ACWhite ""

#define ACPlain ""
#define ACBold ""
#define ACUnderline ""
#define ACBlink ""
#define ACReverse ""

#define ACClear ""
#define ACClearL ""

#define ACCR ""

#define ACSave ""
#define ACRecall ""

#elif __USE_COLORS__ == true

#define ACBlack "\033[0;30m"
#define ACBlue "\033[0;34m"
#define ACGreen "\033[0;32m"
#define ACCyan "\033[0;36m"
#define ACRed "\033[0;31m"
#define ACPurple "\033[0;35m"
#define ACBrown "\033[0;33m"
#define ACGray "\033[0;37m"
#define ACDarkGray "\033[1;30m"
#define ACLightBlue "\033[1;34m"
#define ACLightGreen "\033[1;32m"
#define ACLightCyan "\033[1;36m"
#define ACLightRed "\033[1;31m"
#define ACLightPurple "\033[1;35m"
#define ACYellow "\033[1;33m"
#define ACWhite "\033[1;37m"

#define ACPlain "\033[0m"
#define ACBold "\033[1m"
#define ACUnderline "\033[4m"
#define ACBlink "\033[5m"
#define ACReverse "\033[7m"

#define ACClear "\033[2J"
#define ACClearL "\033[2K"

#define ACCR "\r"

#define ACSave "\033[s"
#define ACRecall "\033[u"

#endif

static std::string ACBlack_       = ACBlack;
static std::string ACBlue_        = ACBlue;
static std::string ACGreen_       = ACGreen;
static std::string ACCyan_        = ACCyan;
static std::string ACRed_         = ACRed;
static std::string ACPurple_      = ACPurple;
static std::string ACBrown_       = ACBrown;
static std::string ACGray_        = ACGray;
static std::string ACDarkGray_    = ACDarkGray;
static std::string ACLightBlue_   = ACLightBlue;
static std::string ACLightGreen_  = ACLightGreen;
static std::string ACLightCyan_   = ACLightCyan;
static std::string ACLightRed_    = ACLightRed;
static std::string ACLightPurple_ = ACLightPurple;
static std::string ACYellow_      = ACYellow;
static std::string ACWhite_       = ACWhite;

static std::string ACPlain_     = ACPlain;
static std::string ACBold_      = ACBold;
static std::string ACUnderline_ = ACUnderline;
static std::string ACBlink_     = ACBlink;
static std::string ACReverse_   = ACReverse;

static std::string ACClear_  = ACClear;
static std::string ACClearL_ = ACClearL;

static std::string ACCR_ = ACCR;

static std::string ACSave_   = ACSave;
static std::string ACRecall_ = ACRecall;

static std::string ACBBlack_       = ACBlack + ACBold_;
static std::string ACBBlue_        = ACBlue + ACBold_;
static std::string ACBGreen_       = ACGreen + ACBold_;
static std::string ACBCyan_        = ACCyan + ACBold_;
static std::string ACBRed_         = ACRed + ACBold_;
static std::string ACBPurple_      = ACPurple + ACBold_;
static std::string ACBBrown_       = ACBrown + ACBold_;
static std::string ACBGray_        = ACGray + ACBold_;
static std::string ACBDarkGray_    = ACDarkGray + ACBold_;
static std::string ACBLightBlue_   = ACLightBlue + ACBold_;
static std::string ACBLightGreen_  = ACLightGreen + ACBold_;
static std::string ACBLightCyan_   = ACLightCyan + ACBold_;
static std::string ACBLightRed_    = ACLightRed + ACBold_;
static std::string ACBLightPurple_ = ACLightPurple + ACBold_;
static std::string ACBYellow_      = ACYellow + ACBold_;
static std::string ACBWhite_       = ACWhite + ACBold_;

namespace DMM
{
static int  __span__            = 30;
static int  __pthr__            = 0;
static bool __enablePrintouts__ = true;
// static bool        __disableColors__    = false  	  ;
static bool        __long__   = true;
static bool        __meth__   = true;
static bool        __file__   = false;
static bool        __date__   = false;
static bool        __time__   = false;
static std::string __c_file__ = ACBCyan_;
static std::string __c_date__ = ACBLightCyan_;
static std::string __c_time__ = ACBLightBlue_;
static std::string __c_meth__ = ACBLightPurple_;

#define __PRE__(priority, iostream)                                                                                 \
	{                                                                                                               \
		if(DMM::__enablePrintouts__)                                                                                \
		{                                                                                                           \
			std::stringstream msg_;                                                                                 \
			std::stringstream ss;                                                                                   \
			std::string       toTrim;                                                                               \
			std::string       PF_;                                                                                  \
			std::stringstream PFs_;                                                                                 \
			std::string       blanks = "";                                                                          \
			int               PFSize = 0;                                                                           \
			int               maxL   = 30;                                                                          \
			int               msgS;                                                                                 \
			int               blankSize;                                                                            \
                                                                                                                    \
			msg_ << ACBCyan_ << __LINE__ << ACPlain << ACYellow << "\t] " << ACPlain;                               \
                                                                                                                    \
			if(DMM::__meth__)                                                                                       \
			{                                                                                                       \
				PF_ = __FUNCTION__;                                                                                 \
				if(DMM::__long__)                                                                                   \
					PF_ = __PRETTY_FUNCTION__;                                                                      \
				PFSize = PF_.size();                                                                                \
				if(PFSize >= DMM::__span__)                                                                         \
				{                                                                                                   \
					PFSize = DMM::__span__;                                                                         \
				}                                                                                                   \
				for(int i = 0; i < PFSize; ++i)                                                                     \
				{                                                                                                   \
					PFs_ << PF_[i];                                                                                 \
				}                                                                                                   \
				if(PFSize < DMM::__span__)                                                                          \
				{                                                                                                   \
					for(int i = 0; i < DMM::__span__ + 3 - PFSize; ++i)                                             \
					{                                                                                               \
						PFs_ << " ";                                                                                \
					}                                                                                               \
				}                                                                                                   \
				if(PFSize < (int)PF_.size())                                                                        \
				{                                                                                                   \
					maxL -= 4;                                                                                      \
					PFs_ << ACRed << "...";                                                                         \
				}                                                                                                   \
				else                                                                                                \
				{                                                                                                   \
					PFs_ << ACPlain << ACPlain << ACPlain;                                                          \
				}                                                                                                   \
				msgS = PFs_.str().size() + 1;                                                                       \
				if(msgS <= DMM::__span__)                                                                           \
					msgS = DMM::__span__;                                                                           \
				blankSize = maxL - msgS;                                                                            \
				if(blankSize < 0)                                                                                   \
					blankSize = 3;                                                                                  \
				for(int i = 0; i < blankSize; ++i)                                                                  \
				{                                                                                                   \
					blanks += " ";                                                                                  \
				}                                                                                                   \
				PFs_ << blanks;                                                                                     \
				msg_ << ACYellow << "[" << DMM::__c_meth__ << PFs_.str() << ACPlain << ACYellow << "] " << ACPlain; \
			}                                                                                                       \
                                                                                                                    \
			if(DMM::__file__)                                                                                       \
			{                                                                                                       \
				PFs_.str("");                                                                                       \
				PF_    = __FILE__;                                                                                  \
				PFSize = PF_.size();                                                                                \
				if(PFSize >= DMM::__span__)                                                                         \
				{                                                                                                   \
					PFSize = DMM::__span__;                                                                         \
				}                                                                                                   \
				for(int i = 0; i < PFSize; ++i)                                                                     \
				{                                                                                                   \
					PFs_ << PF_[i];                                                                                 \
				}                                                                                                   \
				if(PFSize < DMM::__span__)                                                                          \
				{                                                                                                   \
					for(int i = 0; i < DMM::__span__ + 3 - PFSize; ++i)                                             \
					{                                                                                               \
						PFs_ << " ";                                                                                \
					}                                                                                               \
				}                                                                                                   \
				if(PFSize < (int)PF_.size())                                                                        \
				{                                                                                                   \
					maxL -= 4;                                                                                      \
					PFs_ << ACRed << "...";                                                                         \
				}                                                                                                   \
				else                                                                                                \
				{                                                                                                   \
					PFs_ << ACPlain << ACPlain << ACPlain;                                                          \
				}                                                                                                   \
				msgS = PFs_.str().size() + 1;                                                                       \
				if(msgS <= DMM::__span__)                                                                           \
					msgS = DMM::__span__;                                                                           \
				blankSize = maxL - msgS;                                                                            \
				if(blankSize < 0)                                                                                   \
					blankSize = 3;                                                                                  \
				for(int i = 0; i < blankSize; ++i)                                                                  \
				{                                                                                                   \
					blanks += " ";                                                                                  \
				}                                                                                                   \
				PFs_ << blanks;                                                                                     \
				msg_ << ACYellow << "[" << DMM::__c_file__ << PFs_.str() << ACPlain << ACYellow << "] " << ACPlain; \
			}                                                                                                       \
                                                                                                                    \
			if(DMM::__date__)                                                                                       \
			{                                                                                                       \
				msg_ << ACYellow << "[" << DMM::__c_date__ << __DATE__ << ACPlain << ACYellow << "] " << ACPlain;   \
			}                                                                                                       \
                                                                                                                    \
			if(DMM::__time__)                                                                                       \
			{                                                                                                       \
				msg_ << ACYellow << "[" << DMM ::__c_time__ << __TIME__ << ACPlain << ACYellow << "] " << ACPlain;  \
			}                                                                                                       \
			if(priority >= DMM::__pthr__)                                                                           \
			{                                                                                                       \
				std::cout << msg_.str();                                                                            \
				iostream;                                                                                           \
				cout << ACPlain;                                                                                    \
			}                                                                                                       \
		}                                                                                                           \
	}
}  // namespace DMM

#define __PRE0__(iostream) __PRE__(0, iostream)

#endif  // __DM_MACROS__
