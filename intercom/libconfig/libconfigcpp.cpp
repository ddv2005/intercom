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

#include "libconfig.hpp"

#ifdef _MSC_VER
#pragma warning (disable: 4996)
#endif

#include "wincompat.h"
#include "libconfig.h"

#include <cstring>
#include <cstdlib>
#include <sstream>

namespace libconfig {


// ---------------------------------------------------------------------------

static void __constructPath(const Setting *setting,
                            std::stringstream &path)
{
  // head recursion to print path from root to target

	if(setting)
		if(! setting->isRoot())
		{
			__constructPath(setting->getParent(), path);
			if(path.tellp() > 0)
				path << '.';

			const char *name = setting->getName().c_str();
			if(name)
				path << name;
			else
				path << '[' << setting->getIndex() << ']';
		}
}

void Config::ConfigDestructor(void *arg)
{
  delete reinterpret_cast<Setting *>(arg);
}

// ---------------------------------------------------------------------------

Config::Config()
  : _defaultFormat(Setting::FormatDefault)
{
  _config = new config_t;
  config_init(_config);
  config_set_destructor(_config, ConfigDestructor);
}

// ---------------------------------------------------------------------------

Config::~Config()
{
  config_destroy(_config);
  delete _config;
}

// ---------------------------------------------------------------------------

void Config::setAutoConvert(bool flag)
{
  config_set_auto_convert(_config, (flag ? CONFIG_TRUE : CONFIG_FALSE));
}

// ---------------------------------------------------------------------------

bool Config::getAutoConvert() const
{
  return(config_get_auto_convert(_config) != CONFIG_FALSE);
}

// ---------------------------------------------------------------------------

void Config::setDefaultFormat(Setting::Format format)
{
  if(format == Setting::FormatHex)
    _defaultFormat = Setting::FormatHex;
  else
    _defaultFormat = Setting::FormatDefault;

  config_set_default_format(_config, static_cast<short>(_defaultFormat));
}

// ---------------------------------------------------------------------------

void Config::setTabWidth(unsigned short width)
{
  config_set_tab_width(_config, width);
}

// ---------------------------------------------------------------------------

unsigned short Config::getTabWidth() const
{
  return(config_get_tab_width(_config));
}

// ---------------------------------------------------------------------------

void Config::setIncludeDir(const char *includeDir)
{
  config_set_include_dir(_config, includeDir);
}

// ---------------------------------------------------------------------------

const char *Config::getIncludeDir() const
{
  return(config_get_include_dir(_config));
}

// ---------------------------------------------------------------------------

result_t Config::handleError()
{
	result_t result = LCSR_OK;
	char buffer[256];
  switch(config_error_type(_config))
  {
    case CONFIG_ERR_NONE:
    	_error = "";
      break;

    case CONFIG_ERR_PARSE:
    	snprintf(buffer,sizeof(buffer),"Parse error %s:%d %s",config_error_file(_config),
          config_error_line(_config),
          config_error_text(_config));
    	_error = buffer;
    	result = LCSR_ERR_PARSE;
      break;

    case CONFIG_ERR_FILE_IO:
    	_error = "IO error";
    	result = LCSR_ERR_IO;
    	break;

    default:
    	_error = "Unknown error";
    	result = LCSR_ERR_UNKNOWN;
    	break;
  }
  return result;
}

// ---------------------------------------------------------------------------

result_t Config::read(FILE *stream)
{
  if(! config_read(_config, stream))
    return handleError();
  else
  	return LCSR_OK;
}

// ---------------------------------------------------------------------------

result_t Config::readString(const char *str)
{
  if(! config_read_string(_config, str))
    return handleError();
  else
  	return LCSR_OK;
}

// ---------------------------------------------------------------------------

result_t Config::write(FILE *stream) const
{
  config_write(_config, stream);
 	return LCSR_OK;
}

// ---------------------------------------------------------------------------

result_t Config::readFile(const char *filename)
{
  if(! config_read_file(_config, filename))
    return handleError();
  else
  	return LCSR_OK;
}

// ---------------------------------------------------------------------------

result_t Config::writeFile(const char *filename)
{
  if(! config_write_file(_config, filename))
    return handleError();
  else
  	return LCSR_OK;
}

// ---------------------------------------------------------------------------

Setting & Config::getRoot() const
{
  return(*Setting::wrapSetting(config_root_setting(_config)));
}

// ---------------------------------------------------------------------------

Setting::Setting(config_setting_t *setting)
  : _setting(setting)
{
  switch(config_setting_type(setting))
  {
    case CONFIG_TYPE_GROUP:
      _type = TypeGroup;
      break;

    case CONFIG_TYPE_INT:
      _type = TypeInt;
      break;

    case CONFIG_TYPE_INT64:
      _type = TypeInt64;
      break;

    case CONFIG_TYPE_FLOAT:
      _type = TypeFloat;
      break;

    case CONFIG_TYPE_STRING:
      _type = TypeString;
      break;

    case CONFIG_TYPE_BOOL:
      _type = TypeBoolean;
      break;

    case CONFIG_TYPE_ARRAY:
      _type = TypeArray;
      break;

    case CONFIG_TYPE_LIST:
      _type = TypeList;
      break;

    case CONFIG_TYPE_NONE:
    default:
      _type = TypeNone;
      break;
  }

  switch(config_setting_get_format(setting))
  {
    case CONFIG_FORMAT_HEX:
      _format = FormatHex;
      break;

    case CONFIG_FORMAT_DEFAULT:
    default:
      _format = FormatDefault;
      break;
  }
}

// ---------------------------------------------------------------------------

Setting::~Setting()
{
  _setting = NULL;
}

// ---------------------------------------------------------------------------

void Setting::setFormat(Format format)
{
  if((_type == TypeInt) || (_type == TypeInt64))
  {
    if(format == FormatHex)
      _format = FormatHex;
    else
      _format = FormatDefault;
  }
  else
    _format = FormatDefault;

  config_setting_set_format(_setting, static_cast<short>(_format));
}

// ---------------------------------------------------------------------------
bool Setting::getValueAsBool(const char *name) const
{
	return(config_setting_get_bool(_setting) ? true : false);
}
int Setting::getValueAsInt(const char *name) const
{
	return(config_setting_get_int(_setting));
}
long long Setting::getValueAsLongLong(const char *name) const
{
	return(config_setting_get_int64(_setting));
}

float Setting::getValueAsFloat(const char *name) const
{
	return(config_setting_get_float(_setting));
}

std::string  Setting::getValueAsString(const char *name) const
{
  const char *s = config_setting_get_string(_setting);

  std::string str;
  if(s)
    str = s;

  return(str);
}
// ---------------------------------------------------------------------------

Setting * Setting::settingsByIndex(int i) const
{
  if((_type != TypeArray) && (_type != TypeGroup) && (_type != TypeList))
    return NULL;

  config_setting_t *setting = config_setting_get_elem(_setting, i);

  if(! setting)
    return NULL;

  return(wrapSetting(setting));
}

// ---------------------------------------------------------------------------

Setting * Setting::settingsByKey(const char *key) const
{
	if(!isGroup())
		return NULL;
  config_setting_t *setting = config_setting_get_member(_setting, key);

  if(!setting)
    return NULL;

  return(wrapSetting(setting));
}

// ---------------------------------------------------------------------------

bool Setting::exists(const char *name) const
{
  if(_type != TypeGroup)
    return(false);

  config_setting_t *setting = config_setting_get_member(_setting, name);

  return(setting != NULL);
}

// ---------------------------------------------------------------------------

int Setting::getLength() const
{
  return(config_setting_length(_setting));
}

// ---------------------------------------------------------------------------

std::string Setting::getName() const
{
	std::string result = config_setting_name(_setting);
  return result;
}

// ---------------------------------------------------------------------------

std::string Setting::getPath() const
{
  std::stringstream path;

  __constructPath(this, path);

  return(path.str());
}

// ---------------------------------------------------------------------------

const Setting * Setting::getParent() const
{
  config_setting_t *setting = config_setting_parent(_setting);

  if(! setting)
    return NULL;

  return(wrapSetting(setting));
}

// ---------------------------------------------------------------------------

Setting * Setting::getParent()
{
  config_setting_t *setting = config_setting_parent(_setting);

  if(! setting)
    return NULL;

  return(wrapSetting(setting));
}

// ---------------------------------------------------------------------------

unsigned int Setting::getSourceLine() const
{
  return(config_setting_source_line(_setting));
}

// ---------------------------------------------------------------------------

std::string Setting::getSourceFile() const
{
	std::string result = config_setting_source_file(_setting);
  return(result);
}

// ---------------------------------------------------------------------------

bool Setting::isRoot() const
{
  return(config_setting_is_root(_setting));
}

// ---------------------------------------------------------------------------

int Setting::getIndex() const
{
  return(config_setting_index(_setting));
}


// ---------------------------------------------------------------------------

Setting * Setting::wrapSetting(config_setting_t *s)
{
  Setting *setting = NULL;

  void *hook = config_setting_get_hook(s);
  if(! hook)
  {
    setting = new Setting(s);
    config_setting_set_hook(s, reinterpret_cast<void *>(setting));
  }
  else
    setting = reinterpret_cast<Setting *>(hook);

  return setting;
}

// ---------------------------------------------------------------------------

}; // namespace libconfig

// eof
