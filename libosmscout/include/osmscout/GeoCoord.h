#ifndef OSMSCOUT_GEOCOORD_H
#define OSMSCOUT_GEOCOORD_H

/*
  This source is part of the libosmscout library
  Copyright (C) 2013  Tim Teulings

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
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <osmscout/CoreImportExport.h>

#include <string>

#include <osmscout/OSMScoutTypes.h>

#include <osmscout/util/Magnification.h>
#include <osmscout/util/Distance.h>
#include <osmscout/util/Bearing.h>

#include <osmscout/system/Math.h>
#include <osmscout/system/Compiler.h>

namespace osmscout {
  /**
   * \ingroup Util
   * Coordinates will be stored as unsigned long values in file.
   * For the conversion the float value is shifted to positive
   * value and afterwards multiplied by conversion factor
   * to get long values without significant values after colon.
   */
  extern OSMSCOUT_API const double lonConversionFactor;

  /**
   * \ingroup Util
   * Coordinates will be stored as unsigned long values in file.
   * For the conversion the float value is shifted to positive
   * value and afterwards multiplied by conversion factor
   * to get long values without significant values after colon.
   */
  extern OSMSCOUT_API const double latConversionFactor;

  constexpr uint32_t maxRawCoordValue = 0x7FFFFFF; // 134217727

  /**
   * \ingroup Util
   * Number of bytes needed to store a lat,lon coordinate pair.
   */
  const size_t coordByteSize=7;

  /**
   * \ingroup Geometry
   *
   * Anonymous geographic coordinate.
   */
  class OSMSCOUT_API GeoCoord CLASS_FINAL
  {
  private:
    double lat = 0.0;
    double lon = 0.0;

  public:
    /**
     * The default constructor creates an uninitialized instance (for performance reasons).
     */
    inline GeoCoord() = default;

    inline GeoCoord(const GeoCoord& other) = default;

    /**
     * Initialize the coordinate with the given latitude and longitude values.
     */
    inline GeoCoord(double lat,
                    double lon)
     :lat(lat),lon(lon)
    {
      // no code
    }

    /**
     * Assign a new latitude and longitude value to the coordinate
     */
    inline void Set(double lat,
                    double lon)
    {
      this->lat=lat;
      this->lon=lon;
    }

    /**
     * Return the latitude value of the coordinate
     */
    inline double GetLat() const
    {
      return lat;
    }

    /**
     * Return the latitude value of the coordinate
     */
    inline double GetLon() const
    {
      return lon;
    }

    /**
     * Return a string representation of the coordinate value in a human readable format.
     */
    std::string GetDisplayText() const;

    inline std::ostream& operator<<(std::ostream& stream)
    {
      stream << GetDisplayText();
      return stream;
    }

    /**
     * Returns a fast calculable unique id for the coordinate. Coordinates with have
     * the same latitude and longitude value in the supported resolution wil have the same
     * id.
     *
     * The id does not have any semantics regarding sorting. Coordinates with close ids
     * do not need to be close in location.
     */
    Id GetId() const;

    /**
     * Encode the coordinate value into a buffer (with at least a size of coordByteSize).
     */
    inline void EncodeToBuffer(unsigned char buffer[]) const
    {
      uint32_t latValue=(uint32_t)round((lat+90.0)*latConversionFactor);
      uint32_t lonValue=(uint32_t)round((lon+180.0)*lonConversionFactor);

      buffer[0]=((latValue >>  0u) & 0xffu);
      buffer[1]=((latValue >>  8u) & 0xffu);
      buffer[2]=((latValue >> 16u) & 0xffu);

      buffer[3]=((lonValue >>  0u) & 0xffu);
      buffer[4]=((lonValue >>  8u) & 0xffu);
      buffer[5]=((lonValue >> 16u) & 0xffu);

      buffer[6]=((latValue >> 24u) & 0x07u) | ((lonValue >> 20u) & 0x70u);
    }

    /**
     * Encode the coordinate value into a number (the number has hash character).
     */
    inline Id GetHash() const
    {
      uint64_t latValue=(uint64_t)round((lat+90.0)*latConversionFactor);
      uint64_t lonValue=(uint64_t)round((lon+180.0)*lonConversionFactor);
      uint64_t number=0;

      for (size_t i=0; i<27; i++) {
        size_t bit=26-i;

        number=number << 1u;
        number=number+((latValue >> bit) & 0x01u);

        number=number << 1u;
        number=number+((lonValue >> bit) & 0x01u);
      }

      return number;
    }

    /**
     * Decode the coordinate value from a buffer (with at least a size of coordByteSize).
     */
    inline void DecodeFromBuffer(const unsigned char buffer[])
    {
      uint32_t latDat=  (buffer[0] <<  0u)
                      | (buffer[1] <<  8u)
                      | (buffer[2] << 16u)
                      | ((buffer[6] & 0x0fu) << 24u);

      uint32_t lonDat=  (buffer[3] <<  0u)
                      | (buffer[4] <<  8u)
                      | (buffer[5] << 16u)
                      | ((buffer[6] & 0xf0u) << 20u);

      lat=latDat/latConversionFactor-90.0;
      lon=lonDat/lonConversionFactor-180.0;
    }

    /**
     * Return true if both coordinates are equal (using == operator)
     */
    inline bool IsEqual(const GeoCoord& other) const
    {
      return lat==other.lat && lon==other.lon;
    }

    /**
     * Parse a textual representation of a geo coordinate from a string
     * to an GeoCoord instance.
     *
     * The text should follow the following expression:
     *
     * [+|-|N|S] <coordinate> [N|S] [+|-|W|E] <coordinate> [W|E]
     *
     * coordinate may have one of these formats:
     *  DDD[.DDDDD]
     *  DD°[D[.DDD]'[D[.DDD]"]]
     *
     * The means:
     * * You first define the latitude, then the longitude value
     * * You can define with half you mean by either prefixing or postfixing
     * the actual number with a hint
     * * The hint can either be a sign ('-' or '+') or a direction ('N' and 'S'
     * for latitude, 'E' or 'W' for longitude).
     *
     * Possibly in future more variants will be supported. It is guaranteed
     * that the result of GetDisplayText() is successfully parsed.
     *
     * @param text
     *    Text containing the textual representation
     * @param coord
     *    The resulting coordinate, if the text was correctly parsed
     * @return
     *    true, if the text was correctly parsed, else false
     */
    static bool Parse(const std::string& text,
                      GeoCoord& coord);

    /**
     * Get distance between two coordinates.
     * @param target
     *    Target coordinate to measure distance
     * @return
     *    Point to point distance to target coordinates
     * @note
     *    The difference in height between the two points is neglected.
     */
    Distance GetDistance(const GeoCoord &target) const;

    /**
    * Get coordinate of position + course and distance.
    * @param bearing
    *    Target course in degree
    * @param distance
    *    Target distance
    * @return
    *    Target coordinates
    * @note
    *    The difference in height between the two points is neglected.
    */
    GeoCoord Add(const Bearing &bearing, const Distance &distance);

    /**
     * Return true if both coordinates are equals (using == operator)
     */
    inline bool operator==(const GeoCoord& other) const
    {
      return lat==other.lat && lon==other.lon;
    }

    /**
     * Return true if coordinates are not equal
     */
    inline bool operator!=(const GeoCoord& other) const
    {
      return lat!=other.lat || lon!=other.lon;
    }

    inline bool operator<(const GeoCoord& other) const
    {
      return lat<other.lat ||
             (lat==other.lat && lon<other.lon);
    }

    /**
     * Assign the value of other
     */
    inline GeoCoord& operator=(const GeoCoord& other) = default;

    inline Distance operator-(const GeoCoord& other) const
    {
      return GetDistance(other);
    }
  };
}

#endif
