/*
TinyGPS++ - a small GPS library for Arduino providing universal NMEA parsing
Based on work by and "distanceBetween" and "courseTo" courtesy of Maarten Lamers.
Suggestion to add satellites, courseTo(), and cardinal() by Matt Monson.
Location precision improvements suggested by Wayne Holder.
Copyright (C) 2008-2013 Mikal Hart
All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "TinyGPS++.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#define _GPGSVterm   "GPGSV"
#define _GPRMCterm   "GPRMC"
#define _GPGGAterm   "GPGGA"

TinyGPSPlus::TinyGPSPlus()
  :  parity(0)
  ,  flags(FLAG_DEFAULT)
  ,  curSentenceType(GPS_SENTENCE_OTHER)
  ,  curTermNumber(0)
  ,  curTermOffset(0)
  ,  fixQ(0)
#ifndef TINYGPS_OPTION_NO_CUSTOM_FIELDS
  ,  customElts(0)
  ,  customCandidates(0)
#endif
#ifndef TINYGPSPLUS_OPTION_NO_STATISTICS
  ,  encodedCharCount(0)
  ,  sentencesWithFixCount(0)
  ,  failedChecksumCount(0)
  ,  passedChecksumCount(0)
#endif
{
  term[0] = '\0';
}

//
// public methods
//

bool TinyGPSPlus::encode(char c)
{
  ++encodedCharCount;

  switch(c)
  {
  case ',': // term terminators
    parity ^= (uint8_t)c;
    [[fallthrough]];
  case '\r':
  case '\n':
  case '*':
    {
      bool isValidSentence = false;
      if (curTermOffset == 0) {
        term[curTermOffset] = 0;
        isValidSentence = endOfTermHandler(false);

      } else if (curTermOffset < sizeof(term)) {
        term[curTermOffset] = 0;
        isValidSentence = endOfTermHandler(true);
      }
      ++curTermNumber;
      curTermOffset = 0;
      term[curTermOffset] = 0;
      if(c == '*')
      {
        flags |= FLAG_IS_CHECKSUM_TERM;
      }
      else
      {
        flags &= (~FLAG_IS_CHECKSUM_TERM);
      }
      return isValidSentence;
    }
    break;

  case '$': // sentence begin
    sentenceTime = millis();
    curTermNumber = curTermOffset = 0;
    parity = 0;
    curSentenceType = GPS_SENTENCE_OTHER;
    flags &= (~FLAG_IS_CHECKSUM_TERM);
    setSentenceHasFix(false);
    return false;

  default: // ordinary characters
    if (curTermOffset < sizeof(term) - 1)
      term[curTermOffset++] = c;
    if ((flags & FLAG_IS_CHECKSUM_TERM)==0)
      parity ^= c;
    return false;
  }

  return false;
}

int TinyGPSPlus::GGA(char *buf)
{
   *buf = 0;
   char* end = buf;
   if(fixQ == 0)
   {
      end = stpcpy(buf, "$GPGGA,,,,,,,,,,,,,,");
   }
   else
   {
      end += sprintf(
                end,
                //      HH  MM  SS   CS   LAT        NS LON       EW Q  SATS HDOP ALT    GEOID
                "$GPGGA,%02d%02d%02d.%02d,%02d%10.7f,%c,%03d%10.7f,%c,%d,%02d,%.1f,%.3f,M,%.3f,M,,",
                time.hour(),
                time.minute(),
                time.second(),
                time.centisecond(),
                location.rawLat().deg,
                (location.lat() - location.rawLat().deg) * 60,
                (location.rawLng().negative ? 'S' : 'N'),
                location.rawLng().deg,
                (location.lng() - location.rawLng().deg) * 60,
                (location.rawLat().negative ? 'W' : 'E'),
                fixQuality(),
                satellites.value(),
                hdop.hdop(),
                altitude.meters(),
                geoidHeight.meters());
   }

   // Calculate checksum
   char checksum = 0;
   for(char* ptr = buf+1; *ptr; ptr++)
      checksum ^= *ptr;

   end += sprintf(end, "*%02X\x0D\x0A", checksum);
   return end - buf;
}

//
// internal utilities
//
int TinyGPSPlus::fromHex(char a)
{
  if (a >= 'A' && a <= 'F')
    return a - 'A' + 10;
  else if (a >= 'a' && a <= 'f')
    return a - 'a' + 10;
  else
    return a - '0';
}

// static
// Parse a (potentially negative) number with up to 2 decimal digits -xxxx.yy
int32_t TinyGPSPlus::parseDecimal(const char *term)
{
  bool negative = *term == '-';
  if (negative) ++term;
  int32_t ret = 100 * (int32_t)atol(term);
  while (isdigit(*term)) ++term;
  if (*term == '.' && isdigit(term[1]))
  {
    ret += 10 * (term[1] - '0');
    if (isdigit(term[2]))
      ret += term[2] - '0';
  }
  return negative ? -ret : ret;
}

// static
// Parse degrees in that funny NMEA format DDMM.MMMM
void TinyGPSPlus::parseDegrees(const char *term, RawDegrees &deg)
{

  deg.deg = 181; // Set to invalid value
  if (!isdigit(*term) && *term != '.') {
    // An invalid character
    // TODO: Must check if the degree is allowed to start with a decimal point.
    return;
  }

  const uint32_t leftOfDecimal = (uint32_t)atol(term);

  while (isdigit(*term)) {
    ++term;
  }

  if (*term != '.') {
    // Degree must have a decimal point
    return;
  }

  deg.deg = (int16_t)(leftOfDecimal / 100);
  uint16_t minutes = (uint16_t)(leftOfDecimal % 100);
  uint32_t multiplier = 10000000UL;
  uint32_t tenMillionthsOfMinutes = minutes * multiplier;
 
  while (isdigit(*++term))
  {
    multiplier /= 10;
    tenMillionthsOfMinutes += (*term - '0') * multiplier;
  }

  deg.billionths = (5 * tenMillionthsOfMinutes + 1) / 3;
  deg.negative = false;
}

#define COMBINE(sentence_type, term_number) (((unsigned)(sentence_type) << 5) | term_number)

// Processes a just-completed term
// Returns true if new sentence has just passed checksum test and is validated
bool TinyGPSPlus::endOfTermHandler(bool termIsNotEmpty)
{
  // If it's the checksum term, and the checksum checks out, commit
  if ((flags & FLAG_IS_CHECKSUM_TERM) != 0)
  {
    byte checksum = 16 * fromHex(term[0]) + fromHex(term[1]);
    if (checksum == parity)
    {
      passedChecksumCount++;
      if (sentenceHasFix())
        ++sentencesWithFixCount;

      switch(curSentenceType)
      {
      case GPS_SENTENCE_GPRMC:
        if (date.isNotEmpty())
          date.commit(sentenceTime);
        if (time.isNotEmpty())
          time.commit(sentenceTime);
        if (sentenceHasFix())
        {
          if (location.isNotEmpty())
           location.commit(sentenceTime);
          if (speed.isNotEmpty())
           speed.commit(sentenceTime);
          if (course.isNotEmpty())
           course.commit(sentenceTime);
        }
        break;
      case GPS_SENTENCE_GPGGA:
        if (time.isNotEmpty())
          time.commit(sentenceTime);
        if (sentenceHasFix())
        {
          if (location.isNotEmpty())
            location.commit(sentenceTime);
          if (altitude.isNotEmpty())
            altitude.commit(sentenceTime);
          if (geoidHeight.isNotEmpty())
            geoidHeight.commit(sentenceTime);
        }
        if (satellites.isNotEmpty())
          satellites.commit(sentenceTime);
        if (hdop.isNotEmpty())
          hdop.commit(sentenceTime);
        break;
      }

#ifndef TINYGPS_OPTION_NO_CUSTOM_FIELDS
      // Commit all custom listeners of this sentence type
      for (TinyGPSCustom *p = customCandidates; p != NULL && strcmp(p->sentenceName, customCandidates->sentenceName) == 0; p = p->next)
         p->commit(sentenceTime);
#endif
      return true;
    }

    else
    {
      if (curSentenceType == GPS_SENTENCE_GPRMC) {
          // Bad checksum in GPRMC sentence, reset optional variables
          //   to prevent meshtastic-device issue #863
          course.newval = speed.newval = 0;
      }
      ++failedChecksumCount;
    }

    return false;
  }

  // the first term determines the sentence type
  // xxRMC/xxGGA where xx = NMEA Talker ID (GP=GPS, GL=GLONASS, GA=Galileo, GB/BD=Beidou, GN=GNSS)
  if (curTermNumber == 0)
  {
    if (strlen(term) == 5 && !strncmp(term+2, "RMC", 3))
      curSentenceType = GPS_SENTENCE_GPRMC;
    else if (strlen(term) == 5 && !strncmp(term+2, "GGA", 3))
      curSentenceType = GPS_SENTENCE_GPGGA;
    else if (!strcmp(term, _GPGSVterm))
      curSentenceType = GPS_SENTENCE_GPGSV;
    else
      curSentenceType = GPS_SENTENCE_OTHER;

#ifndef TINYGPS_OPTION_NO_CUSTOM_FIELDS
    // Any custom candidates of this sentence type?
    for (customCandidates = customElts; customCandidates != NULL && strcmp(customCandidates->sentenceName, term) < 0; customCandidates = customCandidates->next);
    if (customCandidates != NULL && strcmp(customCandidates->sentenceName, term) > 0)
       customCandidates = NULL;
#endif
    // Serial.printf("%s ENC:%i PAS:%i FAI:%i FIX:%i\n", term, encodedCharCount, passedChecksumCount, failedChecksumCount, sentencesWithFixCount);
    return false;
  }

  //see GPGSV described here http://aprs.gids.nl/nmea/#gsa
  if(curSentenceType == GPS_SENTENCE_GPGSV) {
    switch(curTermNumber)
    {
      case 2:
      {
        // MsgId *should* be 1 based, but some devices (Air530) send 0 when they are still starting up
        int msgId = atoi(term) - 1;  
        if(msgId < 0 || msgId >= TINYGPS_MAX_SATS / 4) {
          trackedSatellitesIndex = -1; // Mark as an invalid message, bogus msgId
        }
        else {
          if(msgId == 0) {
            //reset trackedSatellites
            memset(trackedSatellites, 0, TINYGPS_MAX_SATS * sizeof(TinyGPSTrackedSattelites));
          }
          trackedSatellitesIndex = 4* ((uint8_t) msgId); //4 tracked sats per line
        }
        break;
      }
      case 7:
      case 11:
      case 15:
      case 19:
      {
        if(trackedSatellitesIndex >= 0) {
          size_t satnum = trackedSatellitesIndex + (curTermNumber-4)/4;
          if(satnum < TINYGPS_MAX_SATS)
            trackedSatellites[satnum].strength = (uint8_t)atoi(term);
        }
        break;
      }
      case 4:
      case 8:
      case 12:
      case 16: 
      {
        if(trackedSatellitesIndex >= 0) {
          size_t satnum = trackedSatellitesIndex + (curTermNumber-4)/4;
          if(satnum < TINYGPS_MAX_SATS)
            trackedSatellites[satnum].prn = (uint8_t)atoi(term);
        }
        break;
      }
    }
  }
  else if (curSentenceType != GPS_SENTENCE_OTHER)
    switch(COMBINE(curSentenceType, curTermNumber))
  {
    case COMBINE(GPS_SENTENCE_GPRMC, 1): // Time in both sentences
    case COMBINE(GPS_SENTENCE_GPGGA, 1):
      time.setNotEmpty(termIsNotEmpty);
      time.setTime(term);
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 2): // GPRMC validity
      setSentenceHasFix(term[0] == 'A');
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 3): // Latitude
    case COMBINE(GPS_SENTENCE_GPGGA, 2):
      location.setNotEmpty(termIsNotEmpty);
      location.setLatitude(term);
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 4): // N/S
    case COMBINE(GPS_SENTENCE_GPGGA, 3):
      location.newval.lat.negative = term[0] == 'S';
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 5): // Longitude
    case COMBINE(GPS_SENTENCE_GPGGA, 4):
      location.setNotEmpty(termIsNotEmpty);
      location.setLongitude(term);
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 6): // E/W
    case COMBINE(GPS_SENTENCE_GPGGA, 5):
      location.newval.lng.negative = term[0] == 'W';
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 7): // Speed (GPRMC)
      speed.setNotEmpty(termIsNotEmpty);
      speed.set(term);
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 8): // Course (GPRMC)
      course.setNotEmpty(termIsNotEmpty);
      course.set(term);
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 9): // Date (GPRMC)
      date.setNotEmpty(termIsNotEmpty);
      date.setDate(term);
      break;
    case COMBINE(GPS_SENTENCE_GPGGA, 6): // Fix data (GPGGA)
      setSentenceHasFix(term[0] > '0');
      fixQ = term[0] - '0';
      break;
    case COMBINE(GPS_SENTENCE_GPGGA, 7): // Satellites used (GPGGA)
      satellites.setNotEmpty(termIsNotEmpty);
      satellites.set(term);
      break;
    case COMBINE(GPS_SENTENCE_GPGGA, 8): // HDOP
      hdop.setNotEmpty(termIsNotEmpty);
      hdop.set(term);
      break;
    case COMBINE(GPS_SENTENCE_GPGGA, 9): // Altitude (GPGGA)
      altitude.setNotEmpty(termIsNotEmpty);
      altitude.set(term);
      break;
    case COMBINE(GPS_SENTENCE_GPGGA, 11): // Height over Geoid
      geoidHeight.setNotEmpty(termIsNotEmpty);
      geoidHeight.set(term);
      break;
  }

#ifndef TINYGPS_OPTION_NO_CUSTOM_FIELDS
  // Set custom values as needed
  for (TinyGPSCustom *p = customCandidates; p != NULL && strcmp(p->sentenceName, customCandidates->sentenceName) == 0 && p->termNumber <= curTermNumber; p = p->next)
    if (p->termNumber == curTermNumber)
         p->set(term);
#endif

  return false;
}

/* static */
double TinyGPSPlus::distanceBetween(double lat1, double long1, double lat2, double long2)
{
  // returns distance in meters between two positions, both specified
  // as signed decimal-degrees latitude and longitude. Uses great-circle
  // distance computation for hypothetical sphere of radius 6372795 meters.
  // Because Earth is no exact sphere, rounding errors may be up to 0.5%.
  // Courtesy of Maarten Lamers
  double delta = radians(long1-long2);
  double sdlong = sin(delta);
  double cdlong = cos(delta);
  lat1 = radians(lat1);
  lat2 = radians(lat2);
  double slat1 = sin(lat1);
  double clat1 = cos(lat1);
  double slat2 = sin(lat2);
  double clat2 = cos(lat2);
  delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
  delta = sq(delta);
  delta += sq(clat2 * sdlong);
  delta = sqrt(delta);
  double denom = (slat1 * slat2) + (clat1 * clat2 * cdlong);
  delta = atan2(delta, denom);
  return delta * 6372795;
}

double TinyGPSPlus::courseTo(double lat1, double long1, double lat2, double long2)
{
  // returns course in degrees (North=0, West=270) from position 1 to position 2,
  // both specified as signed decimal-degrees latitude and longitude.
  // Because Earth is no exact sphere, calculated course may be off by a tiny fraction.
  // Courtesy of Maarten Lamers
  double dlon = radians(long2-long1);
  lat1 = radians(lat1);
  lat2 = radians(lat2);
  double a1 = sin(dlon) * cos(lat2);
  double a2 = sin(lat1) * cos(lat2) * cos(dlon);
  a2 = cos(lat1) * sin(lat2) - a2;
  a2 = atan2(a1, a2);
  if (a2 < 0.0)
  {
    a2 += TWO_PI;
  }
  return degrees(a2);
}

const char *TinyGPSPlus::cardinal(double course)
{
  static const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  int direction = (int)((course + 11.25f) / 22.5f);
  return directions[direction % 16];
}

void TinyGPSLocation::commit(uint32_t timestamp)
{
   createTime = timestamp;
   val.lat = newval.lat;
   val.lng = newval.lng;
   flags |= (FLAG_VALID|FLAG_UPDATED);
}

void TinyGPSLocation::setLatitude(const char *term)
{
   TinyGPSPlus::parseDegrees(term, newval.lat);
}

void TinyGPSLocation::setLongitude(const char *term)
{
   TinyGPSPlus::parseDegrees(term, newval.lng);
}

double TinyGPSLocation::lat()
{
   flags &= (~FLAG_UPDATED);
   double ret = (double)val.lat.deg + ((double)val.lat.billionths / (double)1000000000.0);
   return val.lat.negative ? -ret : ret;
}

double TinyGPSLocation::lng()
{
   flags &= (~FLAG_UPDATED);
   double ret = (double)val.lng.deg + ((double)val.lng.billionths / (double)1000000000.0);
   return val.lng.negative ? -ret : ret;
}

void TinyGPSDate::commit(uint32_t timestamp)
{
   if (!isNotNull) {
      flags = FLAG_DEFAULT;
      return;
   }
   createTime = timestamp;
   val = newval;
   flags |= (FLAG_VALID|FLAG_UPDATED);
   isNotNull = false;
}

void TinyGPSTime::commit(uint32_t timestamp)
{
   if (!isNotNull) {
      flags = FLAG_DEFAULT;
      return;
   }
   createTime = timestamp;
   val = newval;
   flags |= (FLAG_VALID|FLAG_UPDATED);
   isNotNull = false;
}

void TinyGPSTime::setTime(const char *term)
{
   isNotNull = true;
   newval = (uint32_t)TinyGPSPlus::parseDecimal(term);
}

void TinyGPSDate::setDate(const char *term)
{
   isNotNull = true;
   newval = atol(term);
}

uint16_t TinyGPSDate::year()
{
   flags &= (~FLAG_UPDATED);
   uint16_t year = val % 100;
   return year + 2000;
}

uint8_t TinyGPSDate::month()
{
   flags &= (~FLAG_UPDATED);
   return (val / 100) % 100;
}

uint8_t TinyGPSDate::day()
{
   flags &= (~FLAG_UPDATED);
   return val / 10000;
}

uint8_t TinyGPSTime::hour()
{
   flags &= (~FLAG_UPDATED);
   return val / 1000000;
}

uint8_t TinyGPSTime::minute()
{
   flags &= (~FLAG_UPDATED);
   return (val / 10000) % 100;
}

uint8_t TinyGPSTime::second()
{
   flags &= (~FLAG_UPDATED);
   return (val / 100) % 100;
}

uint8_t TinyGPSTime::centisecond()
{
   flags &= (~FLAG_UPDATED);
   return val % 100;
}

void TinyGPSDecimal::commit(uint32_t timestamp)
{
   createTime = timestamp;
   val = newval;
   flags |= (FLAG_VALID|FLAG_UPDATED);
}

void TinyGPSDecimal::set(const char *term)
{
   newval = TinyGPSPlus::parseDecimal(term);
}

void TinyGPSAltitude::set(const char *term)
{
   newval = TinyGPSPlus::parseDecimal(term);
}

void TinyGPSAltitude::commit(uint32_t timestamp)
{
   createTime = timestamp;
   val = newval;
   flags |= (FLAG_VALID|FLAG_UPDATED);
}

void TinyGPSInteger::commit(uint32_t timestamp)
{
   createTime = timestamp;
   val = newval;
   flags |= (FLAG_VALID|FLAG_UPDATED);
}

void TinyGPSInteger::set(const char *term)
{
   newval = atol(term);
}

#ifndef TINYGPS_OPTION_NO_CUSTOM_FIELDS
TinyGPSCustom::TinyGPSCustom(TinyGPSPlus &gps, const char *_sentenceName, int _termNumber)
{
   begin(gps, _sentenceName, _termNumber);
}

void TinyGPSCustom::begin(TinyGPSPlus &gps, const char *_sentenceName, int _termNumber)
{
   createTime = millis();
   flags &= (~(FLAG_UPDATED|FLAG_VALID));
   sentenceName = _sentenceName;
   termNumber = _termNumber;
   memset(stagingBuffer, '\0', sizeof(stagingBuffer));
   memset(buffer, '\0', sizeof(buffer));

   // Insert this item into the GPS tree
   gps.insertCustom(this, _sentenceName, _termNumber);
}

void TinyGPSCustom::commit(uint32_t timestamp)
{
   createTime = timestamp;
   strcpy(this->buffer, this->stagingBuffer);
   flags |= (FLAG_VALID|FLAG_UPDATED);
}

void TinyGPSCustom::set(const char *term)
{
   strncpy(this->stagingBuffer, term, sizeof(this->stagingBuffer) - 1);
}

void TinyGPSPlus::insertCustom(TinyGPSCustom *pElt, const char *sentenceName, int termNumber)
{
   TinyGPSCustom **ppelt;

   for (ppelt = &this->customElts; *ppelt != NULL; ppelt = &(*ppelt)->next)
   {
      int cmp = strcmp(sentenceName, (*ppelt)->sentenceName);
      if (cmp < 0 || (cmp == 0 && termNumber < (*ppelt)->termNumber))
         break;
   }

   pElt->next = *ppelt;
   *ppelt = pElt;
}
#endif
