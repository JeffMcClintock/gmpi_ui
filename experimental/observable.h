#pragma once
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace gmpi_forms
{
template <typename T>
struct StateRef;

struct thing
{
	virtual ~thing() = default;
};

template <typename T>
struct State : public thing
{
private:
	T value = {};
	// model-side side-effect callbacks (e.g. mark-document-modified, notify views). add-only:
	// they live as long as the owning model object, which owns this State as a member.
	std::vector<std::function<void(void)>> observers;

public:
	// bound StateRefs (the view layer). auto-managed: a StateRef (un)registers itself via
	// setSource/release. Fired BEFORE 'observers' so cheap view refreshes (setDirty) run before
	// model side-effects that may rebuild (and thus destroy) those same views.
	std::vector<StateRef<T>*> watchers;

	State() = default;
	State(const T& initialValue) : value(initialValue) {}

	void set(const T& v)
	{
		if(value == v)
			return;

		value = v;

		for (auto& c : watchers)
			c->onChanged();

		for (auto& o : observers)
			o();
	}

	void set(T&& v)
	{
		if (value == v)
			return;

		value = std::move(v);

		for (auto& c : watchers)
			c->onChanged();

		for (auto& o : observers)
			o();
	}

	State& operator=(const T& v)
	{
		set(v);
		return *this;
	}

	State& operator=(T&& v)
	{
		set(std::move(v));
		return *this;
	}

	const T& get() const
	{
		return value;
	}

	void release(StateRef<T>* s)
	{
		watchers.erase(
			std::remove_if(watchers.begin(), watchers.end(),
				[s](const auto* o) { return o == s; }),
			watchers.end());
	}

	explicit operator const T&() const
	{
		return value;
	}

	void addObserver(std::function<void(void)> callback)
	{
		observers.push_back(std::move(callback));
	}
};

// an observer of a state
template <typename T>
struct StateRef : public thing
{
	StateRef() = default;
	StateRef(State<T>* pstate) : state(pstate) {}
	~StateRef()
	{
		if (state)
			state->release(this);
	}

	const T& get() const
	{
		return state->get();
	}
	explicit operator const T& () const
	{
		return state->get();
	}

	// Read-only handle: a StateRef observes/displays a State; it cannot write it directly.
	// UI -> model goes through a widget's 'validateAndSave' back-channel (which calls the model's
	// own setter). A widget that genuinely owns/drives its source state writes it explicitly via
	// getSource()->set().

	void addObserver(std::function<void(void)> callback)
	{
		callbacks.push_back(callback);
	}

	void onChanged()
	{
		for (auto& c : callbacks)
			c();
	}

	void setSource(State<T>* newStateHolder) {
		if (state)
			state->release(this);
		state = newStateHolder;
		if (state)
			state->watchers.push_back(this);
	}

	State<T>* getSource() const {
		return state;
	}

protected:
	State<T>* state = {};
	std::vector < std::function<void(void)> > callbacks;
};
} // namespace gmpi_forms
