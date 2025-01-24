#ifndef _ots_DataStructs_h
#define _ots_DataStructs_h

#include "otsdaq/Macros/CoutMacros.h"

#include <map>
#include <set>

namespace ots
{
template<typename N, typename V>
class Parameter
{
  public:
	Parameter(N name, V value) : name_(name), value_(value) { ; }
	virtual ~Parameter(void) { ; }
	// Getters
	const N&              getName(void) const { return name_; }
	const V&              getValue(void) const { return value_; }
	const std::pair<N, V> getParameterPair(void)
	{
		return std::pair<N, V>(name_, value_);
	}

	// Setters
	void setName(const N name) { name_ = name; }
	void setValue(const V value) { value_ = value; }
	void set(const N name, const V& value)
	{
		name_  = name;
		value_ = value;
	}
	void set(std::pair<N, V> parameter)
	{
		name_  = parameter.first;
		value_ = parameter.second;
	}

  protected:
	N name_;
	V value_;
};

template<typename N, typename V>
class Parameters
{
  public:
	typedef std::map<N, V>                        ParameterMap;
	typedef typename ParameterMap::iterator       iterator;
	typedef typename ParameterMap::const_iterator const_iterator;

	Parameters(void) { ; }
	Parameters(N name, V value) { addParameter(name, value); }
	Parameters(Parameter<N, V> parameter) { addParameter(parameter); }
	virtual ~Parameters(void) { ; }
	// Getters
	std::set<N> getNames(void) const
	{
		std::set<N> names;
		for(const_iterator it = theParameters_.begin(); it != theParameters_.end(); it++)
			names.insert(it->first);
		return names;
	}
	const V& getValue(const N name) const
	{
		auto it = theParameters_.find(name);
		if(it == theParameters_.end())
		{
			__SS__ << "Parameter '" << name << "' not found!" << __E__;
			__SS_ONLY_THROW__;
		}
		return it->second;
	}
	Parameter<N, V> getParameter(const N name)
	{
		return Parameter<N, V>(name, getValue(name));
	}

	// Setters
	void addParameter(const N name, const V value) { theParameters_[name] = value; }
	void addParameter(const Parameter<N, V> parameter)
	{
		theParameters_[parameter.getName()] = parameter.getValue();
	}
	void addParameter(const std::pair<N, V> parameterPair)
	{
		theParameters_[parameterPair.first] = parameterPair.second;
	}

	// Iterators
	iterator       begin(void) { return theParameters_.begin(); }
	iterator       end(void) { return theParameters_.end(); }
	const_iterator begin(void) const { return theParameters_.begin(); }
	const_iterator end(void) const { return theParameters_.end(); }
	iterator       find(N name) { return theParameters_.find(name); }
	const_iterator find(N name) const { return theParameters_.find(name); }

	// Methods
	unsigned int size(void) const { return theParameters_.size(); }
	void         clear(void) { theParameters_.clear(); }

  protected:
	ParameterMap theParameters_;
};

}  // namespace ots
#endif
