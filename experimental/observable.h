#pragma once
#include <cassert>
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

	// A StateRef that outlives its State is a GUARANTEED use-after-free: its
	// destructor (or next setSource) calls release() on this object after it
	// is gone. Measured (TIDE BACKLOG E66): SettingsPane::reload() destroyed
	// its MIDI tick-box States while the old widget tree still held StateRefs
	// into them, and the crash surfaced two frames later inside the STL's
	// iterator machinery -- about as far from the cause as it could land.
	//
	// So death does both halves, deliberately in this order:
	//   ASSERT (Debug)  -- the owner broke the teardown contract (visuals
	//                      before states; ViewParent::clear()'s own comment
	//                      states it). The defect stays loud AT THE CAUSE.
	//   DETACH (always) -- null every surviving watcher's back-pointer, so a
	//                      Release build loses a notification instead of
	//                      corrupting the heap. Containment, not a licence:
	//                      the assert above is what keeps it from becoming
	//                      plaster over the root cause.
	~State()
	{
		assert(watchers.empty() && "a StateRef outlives this State - release the visuals BEFORE the states they point to");
		for (auto* w : watchers)
			w->state = nullptr;
	}

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
		callbacks.push_back(std::move(callback));
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
	// ~State() nulls this on any watcher that outlives it -- see there.
	friend struct State<T>;
	State<T>* state = {};
	std::vector < std::function<void(void)> > callbacks;
};
} // namespace gmpi_forms
