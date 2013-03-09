/* ----------------------------------------------------------------------------
   libconfig - A library for processing structured configuration files
   Copyright (C) 2005-2010  Mark A Lindner

   This file is part of libconfig.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, see
   <http://www.gnu.org/licenses/>.
   ----------------------------------------------------------------------------
*/

#ifndef __libconfig_hpp
#define __libconfig_hpp

#include "../project_common.h"
#include <stdio.h>
#include <string>

#define LIBCONFIGXX_VER_MAJOR    1
#define LIBCONFIGXX_VER_MINOR    4
#define LIBCONFIGXX_VER_REVISION 8

struct config_t; // fwd decl
struct config_setting_t; // fwd decl

SWIG_BEGIN_DECL
namespace libconfig {


typedef	int	result_t;
#define LCSR_OK		0
#define LCSR_ERR_IO			-1
#define LCSR_ERR_PARSE	-2
#define LCSR_ERR_UNKNOWN	-100
class  Setting
{
  friend class Config;

  public:

  enum Type
  {
    TypeNone = 0,
    // scalar types
    TypeInt,
    TypeInt64,
    TypeFloat,
    TypeString,
    TypeBoolean,
    // aggregate types
    TypeGroup,
    TypeArray,
    TypeList
  };

  enum Format
  {
    FormatDefault = 0,
    FormatHex = 1
  };

  private:

  config_setting_t *_setting;
  Type _type;
  Format _format;

  Setting(config_setting_t *setting);
  static Setting *wrapSetting(config_setting_t *s);

  Setting(const Setting& other); // not supported
  Setting& operator=(const Setting& other); // not supported

  public:

  virtual ~Setting();

  inline Type getType() const { return(_type); }

  inline Format getFormat() const { return(_format); }
  void setFormat(Format format);

  Setting * settingsByKey(const char *key) const;

  Setting * settingsByIndex(int index) const;

  bool getValueAsBool(const char *name,bool defValue) const;
  int getValueAsInt(const char *name,int defValue) const;
  long long getValueAsLongLong(const char *name,long long defValue) const;
  double getValueAsFloat(const char *name,double defValue) const;
  std::string  getValueAsString(const char *name,const char* defValue) const;

  bool exists(const char *name) const;

  int getLength() const;
  std::string getName() const;
  std::string getPath() const;
  int getIndex() const;

  const Setting * getParent() const;
  Setting * getParent();

  bool isRoot() const;

  inline bool isGroup() const
  { return(_type == TypeGroup); }

  inline bool isArray() const
  { return(_type == TypeArray); }

  inline bool isList() const
  { return(_type == TypeList); }

  inline bool isAggregate() const
  { return(_type >= TypeGroup); }

  inline bool isScalar() const
  { return((_type > TypeNone) && (_type < TypeGroup)); }

  inline bool isNumber() const
  { return((_type == TypeInt) || (_type == TypeInt64)
           || (_type == TypeFloat)); }

  unsigned int getSourceLine() const;
  std::string getSourceFile() const;
};

class  Config
{
  private:

	std::string _error;
  config_t *_config;

  static void ConfigDestructor(void *arg);
  Config(const Config& other); // not supported
  Config& operator=(const Config& other); // not supported

  public:

  Config();
  virtual ~Config();

  void setAutoConvert(bool flag);
  bool getAutoConvert() const;

  void setDefaultFormat(Setting::Format format);
  inline Setting::Format getDefaultFormat() const
  { return(_defaultFormat); }

  void setTabWidth(unsigned short width);
  unsigned short getTabWidth() const;

  void setIncludeDir(const char *includeDir);
  const char *getIncludeDir() const;

  result_t read(FILE *stream);
  result_t write(FILE *stream) const;

  result_t readString(const char *str);


  result_t readFile(const char *filename);
  result_t writeFile(const char *filename);

  Setting & getRoot() const;
  const std::string & getError() const { return _error;}

  private:

  Setting::Format _defaultFormat;

  result_t handleError();
};

} // namespace libconfig
SWIG_END_DECL

#endif // __libconfig_hpp
