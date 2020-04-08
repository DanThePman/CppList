#pragma once
#include <vector>
#include <functional>
#include <algorithm>

#define ForPointerList std::enable_if_t<std::is_pointer_v<TDummy>>* = nullptr  //NOLINT
#define ForObjectList std::enable_if_t<!std::is_pointer_v<TDummy>>* = nullptr  //NOLINT
#define ForIntegrals std::enable_if_t<std::is_integral_v<IntegralDummy>>* = nullptr	 //NOLINT
#define IsBaseOf(TBase, TDerived) std::enable_if_t<std::is_base_of_v<TBase, TDerived>>* = nullptr   //NOLINT

/*
 * Example usage:
 * List<int> numbers { 1, 2, 3 }; //involkes ctor List<int>::List(std::initializer_list<int>& list)
 * auto& object_Reference_For_Integer_Number_2 = numbers.FirstOrDefault([](auto& currentNumberByRef) { return currentNumberByRef == 2; }); //also works using pass by object (auto currentNumberByRef or int currentNumberByRef etc..)
 * assert(object_Reference_For_Integer_Number_2 == 2);
 */

/* Features:
 * Performance and RAM optimized C# Linq expressions
 * Contstructors for plain arrays
 * Lambda bodys with automatic type deduction and lvalue ref support
 * Compile time error support
 * EraseIf-Linq (filter method)
 * Contains (filter method)
 */

/**
 * \tparam T needs to declare a default constructor
 */
template <typename T>
class List : public std::vector<T>  // NOLINT
{
private:
	using Base = std::vector<T>;
	using NormalType = std::remove_pointer_t<T>;
	NormalType DefaultType{};
public:
	List() {}  // NOLINT
	~List() = default;

	List(int currentCount) : Base(currentCount)
	{
	}

	List(const List<T>& lValue) : Base(lValue)
	{
	}

	List(std::vector<T>& vector) : Base(vector)
	{
	}
	
	List(std::initializer_list<T>& list) : Base(list)
	{
	}
	
	List(std::initializer_list<T>&& list) : Base(std::forward<decltype(list)>(list))
	{
	}

	/**
	 * \brief Used for: List<T> { T array[N] } where array is an lvalue
	 */
	template <int N>
	List(T (&arr)[N]) : Base(N)
	{
		memcpy(this->data(), &arr, sizeof(T));
	}

	/**
	 * \brief Used for: List<T> { T* array, int count }
	 */
	template <typename Integral, typename IntegralDummy = Integral>
	List(T* pArray, Integral count, ForIntegrals) : Base(count)
	{
		memcpy(this->data(), pArray, count * sizeof(T));
	}

	/**
	 * \brief Used for: List<Derived> { Base* array, int count } where Derived is an object
	 */
	template <typename TBase, typename Integral, typename IntegralDummy = Integral, typename TBaseDummy = TBase, typename TDummy = T>
	List(TBase* pArray, Integral count, IsBaseOf(TBaseDummy, NormalType), ForIntegrals, ForObjectList) : Base(count)
	{
		NormalType derivedObject{};
		for (auto i = 0; i < count; i++)
		{
			memcpy(&derivedObject, &pArray[i], sizeof(TBase));
			(*this)[i] = derivedObject;
		}
	}

	/**
	 * \brief Used for: List<Derived*> { Base** array, int count }. Requirement: sizeof(Derived) == sizeof(Base), because List<T> does not use dynamic allocations
	 */
	template <typename TBase, typename Integral, typename IntegralDummy = Integral, typename TBaseObject = std::remove_pointer_t<TBase>, typename TBaseDummy = TBase, typename TDummy = T>
	List(TBase* pArray, Integral count, IsBaseOf(TBaseObject, NormalType), ForIntegrals, ForPointerList, std::enable_if_t<std::is_pointer_v<TBaseDummy>>* = nullptr) : Base(count)
	{
		static_assert(sizeof(NormalType) == sizeof(TBaseObject));
		memcpy(this->data(), pArray, count * sizeof(TBase));
	}
	
	/*used for std::map*/
	List& operator=(const List& lValue)
	{
		Base::operator=(lValue);
		return *this;
	}

	/*provided for derived classes of List*/
	List& operator=(List&& rValue) noexcept
	{
		Base::operator=(std::forward<decltype(rValue)>(rValue));
		return *this;
	}

	template <class... Args>
	decltype(auto) Add(Args&&... data)
	{
		return this->emplace_back(std::forward<Args>(data)...);
	}

	void Clear()
	{
		this->clear();
	}

	template <typename TDummy = T>
	_NODISCARD T& Get(int at, ForObjectList)
	{
		return (*this)[at];
	}

	template <typename TDummy = T>
	_NODISCARD NormalType& Get(int at, ForPointerList)
	{
		return *((*this)[at]);
	}

	template <typename TDummy = T>
	_NODISCARD NormalType& GetOrDefault(int at)
	{
		if (at + 1 > Count())
			return DefaultType;
		
		return Get(at);
	}
	
	/**
		\brief Only supports lvalue lists due to pointer expiration
		\return a pointer list
	*/
	template <typename Predicate, typename TDummy = T>
	_NODISCARD List<T*> Where(Predicate&& predicate, ForObjectList) &
	{
		List<T*> result;
		WhereInternal<false>(result, predicate);
		return result;
	}

	template <typename Predicate, typename TDummy = T>
	_NODISCARD List<T*> Where(Predicate&& predicate, ForObjectList) &&
	{
		static_assert(sizeof(T) < 0, "Never call List<T>::Where(Predicate) on object rvalue lists due to pointer expiration");
		throw;
	}
	
	/**
		\return a pointer list
	*/
	template <typename Predicate, typename TDummy = T>
	_NODISCARD List<T> Where(Predicate&& predicate, ForPointerList)
	{
		List<T> result;
		WhereInternal<true>(result, predicate);
		return result;
	}

	template <typename TDummy = T>
	_NODISCARD NormalType* FirstOrNullptr(ForObjectList)
	{
		return Any() ? &this->at(0) : nullptr;
	}

	template <typename TDummy = T>
	_NODISCARD NormalType* FirstOrNullptr(ForPointerList)
	{
		return Any() ? this->at(0) : nullptr;
	}

	template <typename Predicate, typename TDummy = T>
	_NODISCARD NormalType* FirstOrNullptr(Predicate&& predicate, ForObjectList)
	{
		return FirstOrNullptrInternal<false>(predicate);
	}

	template <typename Predicate, typename TDummy = T>
	_NODISCARD NormalType* FirstOrNullptr(Predicate&& predicate, ForPointerList)
	{
		return FirstOrNullptrInternal<true>(predicate);
	}
	
	template <typename TDummy = T>
	_NODISCARD NormalType& FirstOrDefault(ForObjectList)
	{
		return Any() ? this->at(0) : DefaultType;
	}

	template <typename TDummy = T>
	_NODISCARD NormalType& FirstOrDefault(ForPointerList)
	{
		return Any() && this->at(0) ? *this->at(0) : DefaultType;
	}
	
	template <typename Predicate, typename TDummy = T>
	_NODISCARD NormalType& FirstOrDefault(Predicate&& predicate, ForObjectList)
	{
		return FirstOrDefaultInternal<false>(predicate);
	}

	template <typename Predicate, typename TDummy = T>
	_NODISCARD NormalType& FirstOrDefault(Predicate&& predicate, ForPointerList)
	{
		return FirstOrDefaultInternal<true>(predicate);
	}

	/*sorts permanently*/
	template <typename SortDelegate, typename TDummy = T>
	List<T>& OrderBy(SortDelegate&& sortDelegate, ForObjectList)
	{
		OrderByInternal<false>(sortDelegate);
		return *this;
	}

	/*sorts permanently*/
	template <typename SortDelegate, typename TDummy = T>
	List<T>& OrderBy(SortDelegate&& sortDelegate, ForPointerList)
	{
		OrderByInternal<true>(sortDelegate);
		return *this;
	}

	/*sorts permanently*/
	template <typename SortDelegate, typename TDummy = T>
	List<T>& OrderByDescending(SortDelegate&& sortDelegate, ForObjectList)
	{
		OrderByDescendingInternal<false>(sortDelegate);
		return *this;
	}

	/*sorts permanently*/
	template <typename SortDelegate, typename TDummy = T>
	List<T>& OrderByDescending(SortDelegate&& sortDelegate, ForPointerList)
	{
		OrderByDescendingInternal<true>(sortDelegate);
		return *this;
	}

	_NODISCARD bool Any()
	{
		return !this->empty();
	}
	
	template <typename Predicate, typename TDummy = T>
	_NODISCARD bool Any(Predicate&& predicate, ForObjectList)
	{
		return AnyInternal<false, false>(predicate);
	}

	template <typename Predicate, typename TDummy = T>
	_NODISCARD bool Any(Predicate&& predicate, ForPointerList)
	{
		return AnyInternal<true, false>(predicate);
	}

	template <typename Predicate, typename TDummy = T>
	_NODISCARD bool All(Predicate&& predicate, ForObjectList)
	{
		return !AnyInternal<false, true>(predicate);
	}

	template <typename Predicate, typename TDummy = T>
	_NODISCARD bool All(Predicate&& predicate, ForPointerList)
	{
		return !AnyInternal<true, true>(predicate);
	}

	template <typename TDummy = T>
	_NODISCARD bool Contains(TDummy&& element)
	{
		return Any([&](T& currentElement) { return currentElement == element; });
	}

	_NODISCARD int Count()
	{
		return this->size();
	}

	template <typename Predicate, typename TDummy = T>
	_NODISCARD int Count(Predicate&& predicate, ForObjectList)
	{
		return CountInternal<false>(predicate);
	}

	template <typename Predicate, typename TDummy = T>
	_NODISCARD int Count(Predicate&& predicate, ForPointerList)
	{
		return CountInternal<true>(predicate);
	}

	/*be careful when taking pointers from rvalue lists*/
	template <typename SelectorDelegate, typename TDummy = T>
	_NODISCARD auto Select(SelectorDelegate&& selectorDelegate, ForObjectList)
	{
		return SelectInternal<false>(selectorDelegate);
	}

	template <typename SelectorDelegate, typename TDummy = T>
	_NODISCARD auto Select(SelectorDelegate&& selectorDelegate, ForPointerList)
	{
		return SelectInternal<true>(selectorDelegate);
	}

	/*low performance method*/
	template <typename TDummy = T>
	_NODISCARD List<NormalType> ToObjectList(ForPointerList)
	{
		return Select([](NormalType& object) { return object; });
	}

	template <typename TDummy = T>
	_NODISCARD NormalType Sum(ForObjectList)
	{
		return SumInternal<false>();
	}

	template <typename TDummy = T>
	_NODISCARD NormalType Sum(ForPointerList)
	{
		return SumInternal<true>();
	}

	/*returns DBL_MAX by default*/
	template <typename Predicate, typename TDummy = T>
	_NODISCARD double Min(Predicate&& predicate, ForObjectList)
	{
		return MinInternal<false>(predicate);
	}

	/*returns DBL_MAX by default*/
	template <typename Predicate, typename TDummy = T>
	_NODISCARD double Min(Predicate&& predicate, ForPointerList)
	{
		return MinInternal<true>(predicate);
	}

	template <typename Predicate>
	void EraseIf(Predicate&& predicate) &
	{
		for (auto iterator = this->begin(); iterator < this->end();)
		{
			if (predicate(*iterator))
				iterator = this->erase(iterator);
			else
				++iterator;
		}
	}

	bool EraseAt(int index)
	{
		auto i = 0;
		for (auto iterator = this->begin(); iterator < this->end(); ++iterator, i++)
		{
			if (index == i)
			{
				this->erase(iterator);
				return true;
			}
		}

		return false;
	}
private:
	template <bool IsPointerList, typename U, typename Predicate>
	void WhereInternal(List<U>& refList, Predicate&& predicate)
	{
		for (auto i = 0; i < this->size(); i++)
		{
			auto& currentEntry = this->at(i);
			if constexpr (IsPointerList)
			{
				if (predicate(*currentEntry))
					refList.push_back(currentEntry);
			}
			else
			{
				if (predicate(currentEntry))
					refList.push_back(&currentEntry);
			}
		}
	}

	template <bool IsPointerList, typename Predicate>
	NormalType* FirstOrNullptrInternal(Predicate&& predicate)
	{
		for (auto i = 0; i < this->size(); i++)
		{
			auto& currentEntry = this->at(i);
			if constexpr (IsPointerList)
			{
				if (predicate(*currentEntry))
					return currentEntry;
			}
			else
			{
				if (predicate(currentEntry))
					return &currentEntry;
			}
		}

		return nullptr;
	}

	template <bool IsPointerList, typename Predicate>
	NormalType& FirstOrDefaultInternal(Predicate&& predicate)
	{
		for (auto i = 0; i < this->size(); i++)
		{
			auto& currentEntry = this->at(i);
			if constexpr (IsPointerList)
			{
				if (predicate(*currentEntry))
					return *currentEntry;
			}
			else
			{
				if (predicate(currentEntry))
					return currentEntry;
			}
		}

		return DefaultType;
	}

	template <bool IsPointerList, typename SortDelegate>
	void OrderByInternal(SortDelegate&& sortDelegate)
	{
		if constexpr (IsPointerList)
		{
			std::sort(this->begin(), this->end(), [&](T first, T second)
			{
				if (first == nullptr || second == nullptr)
					return false;

				auto result1 = sortDelegate(*first);
				auto result2 = sortDelegate(*second);

				return result1 < result2;
			});
		}
		else
		{
			std::sort(this->begin(), this->end(), [&](T& first, T& second)
			{
				auto result1 = sortDelegate(first);
				auto result2 = sortDelegate(second);

				return result1 < result2;
			});
		}
	}

	template <bool IsPointerList, typename SortDelegate>
	void OrderByDescendingInternal(SortDelegate&& sortDelegate)
	{
		if constexpr (IsPointerList)
		{
			std::sort(this->begin(), this->end(), [&](T first, T second)
			{
				if (first == nullptr || second == nullptr)
					return false;

				auto result1 = sortDelegate(*first);
				auto result2 = sortDelegate(*second);

				return result1 > result2;
			});
		}
		else
		{
			std::sort(this->begin(), this->end(), [&](T& first, T& second)
			{
				auto result1 = sortDelegate(first);
				auto result2 = sortDelegate(second);

				return result1 > result2;
			});
		}
	}
	
	template <bool IsPointerList, bool InvertPredicate, typename Predicate>
	bool AnyInternal(Predicate&& predicate)
	{
		for (auto i = 0; i < this->size(); i++)
		{
			auto& currentEntry = this->at(i);
			if constexpr (IsPointerList)
			{
				if constexpr (InvertPredicate)
				{
					if (!predicate(*currentEntry))
						return true;
				}
				else
				{
					if (predicate(*currentEntry))
						return true;
				}
			}
			else
			{
				if constexpr (InvertPredicate)
				{
					if (!predicate(currentEntry))
						return true;
				}
				else
				{
					if (predicate(currentEntry))
						return true;
				}
			}
		}

		return false;
	}

	template <bool IsPointerList, typename Predicate>
	int CountInternal(Predicate&& predicate)
	{
		auto count = 0;
		for (auto i = 0; i < this->size(); i++)
		{
			auto& currentEntry = this->at(i);
			if constexpr (IsPointerList)
			{
				if (predicate(*currentEntry))
					count++;
			}
			else
			{
				if (predicate(currentEntry))
					count++;
			}
		}

		return count;
	}

	template <bool IsPointerList, typename SelectorDelegate>
	auto SelectInternal(SelectorDelegate&& selectorDelegate)
	{
		auto currentSize = this->size();
		List<decltype(selectorDelegate(DefaultType))> result(currentSize);

		for (auto i = 0; i < currentSize; i++)
		{
			auto& currentEntry = this->at(i);
			if constexpr (IsPointerList)
			{
				result[i] = selectorDelegate(*currentEntry);
			}
			else
			{
				result[i] = selectorDelegate(currentEntry);
			}
		}

		return result;
	}
	
	template <bool IsPointerList>
	NormalType SumInternal()
	{
		NormalType sum = 0;
		for (auto i = 0; i < this->size(); i++)
		{
			auto& currentEntry = this->at(i);
			if constexpr (IsPointerList)
			{
				sum += *currentEntry;
			}
			else
			{
				sum += currentEntry;
			}
		}

		return sum;
	}

	template <bool IsPointerList, typename Predicate>
	double MinInternal(Predicate&& predicate)
	{
		auto minValue = DBL_MAX;
		for (auto i = 0; i < this->size(); i++)
		{
			auto& currentEntry = this->at(i);
			if constexpr (IsPointerList)
			{
				minValue = min(minValue, predicate(*currentEntry));
			}
			else
			{
				minValue = min(minValue, predicate(currentEntry));
			}
		}

		return minValue;
	}
};
