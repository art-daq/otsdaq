/*===============================================================================
 * Monicelli: the FERMILAB MTEST geometry builder and track reconstruction tool
 *
 * Copyright (C) 2014
 *
 * Authors:
 *
 * Dario Menasce      (INFN)
 * Luigi Moroni       (INFN)
 * Jennifer Ngadiuba  (INFN)
 * Stefano Terzo      (INFN)
 * Lorenzo Uplegger   (FNAL)
 * Luigi Vigani       (INFN)
 *
 * INFN: Piazza della Scienza 3, Edificio U2, Milano, Italy 20126
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ================================================================================*/

#ifndef _ANSICOLORS_H
#define _ANSICOLORS_H

#include <iostream>
#include <sstream>

/*! \file ANSIColors.h
 *  \brief Header file containing macros to highlight output lines with color.
 *
 *  Check also <A HREF="http://www.bluesock.org/~willg/dev/ansi.html">http://www.bluesock.org/~willg/dev/ansi.html</A>
 *
 *  Below is a usage example:
 *  <p><b>
 *  \code
 *   #include <ANSIColors.h>"
 *   ...
 *   cout << ACRed << ACBold << "This text will be highlighted in red" << ACPlain << endl ;
 *  \x1Bndcode
 *   </b>
 */

#define ACBlack "\x1B[0;30m"
#define ACBlue "\x1B[0;34m"
#define ACGreen "\x1B[0;32m"
#define ACCyan "\x1B[0;36m"
#define ACRed "\x1B[0;31m"
#define ACPurple "\x1B[0;35m"
#define ACBrown "\x1B[0;33m"
#define ACGray "\x1B[0;37m"
#define ACDarkGray "\x1B[1;30m"
#define ACLightBlue "\x1B[1;34m"
#define ACLightGreen "\x1B[1;32m"
#define ACLightCyan "\x1B[1;36m"
#define ACLightRed "\x1B[1;31m"
#define ACLightPurple "\x1B[1;35m"
#define ACYellow "\x1B[1;33m"
#define ACWhite "\x1B[1;37m"

#define ACPlain "\x1B[0m"
#define ACBold "\x1B[1m"
#define ACUnderline "\x1B[4m"
#define ACBlink "\x1B[5m"
#define ACReverse "\x1B[7m"

#define ACClear "\x1B[2J"
#define ACClearL "\x1B[2K"

#define ACCR " \r"

#define ACSave "\x1B[s"
#define ACRecall "\x1B[u"

#endif  // _ANSICOLORS_H
