#include <gtest/gtest.h>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <cstdio>

#include "experimental/forms.h"

namespace UnitTestGmpiForms
{

class TestUI : public ::testing::Test
{
};

TEST_F(TestUI, GridVerticalStacks)
{
	gmpi::ui::builder::ViewParent builder;
	gmpi::ui::builder::ThreadLocalCurrentBuilder = &builder;
	{
		gmpi::ui::builder::Grid::Initializer spec;
		spec.gap = 10.0f;

		gmpi::drawing::Rect bounds{ 0.0f, 0.0f, 100.0f, 100.0f };

		gmpi::ui::Grid grid(spec, bounds);
			gmpi::ui::Label first("First");
			gmpi::ui::Label second("Second");
	}

	builder.doChildLayout();

	ASSERT_EQ(1u, builder.childViews.size()); // one child, the grid.
	auto gridView = dynamic_cast<gmpi::ui::builder::Grid*>(builder.childViews[0].get());
	ASSERT_NE(gridView, nullptr);
	auto& views = gridView->childViews;

	const auto b0 = views[0]->getBounds();
	const auto b1 = views[1]->getBounds();

	// rowHeight = (100 - 10) / 2 = 45
	EXPECT_FLOAT_EQ(0.0f, b0.top);
	EXPECT_FLOAT_EQ(45.0f, b0.bottom);

	EXPECT_FLOAT_EQ(55.0f, b1.top);
	EXPECT_FLOAT_EQ(100.0f, b1.bottom);
}

TEST_F(TestUI, GridAutoRowsUsesFixedHeightAndResizesBottomToFitChildren)
{
	gmpi::ui::builder::ViewParent builder;
	gmpi::ui::builder::ThreadLocalCurrentBuilder = &builder;

	gmpi::ui::builder::Grid::Initializer spec;
	spec.gap = 5.0f;
	spec.auto_rows = 20.0f;

	gmpi::drawing::Rect bounds{ 0.0f, 10.0f, 200.0f, 999.0f };

	gmpi::ui::Grid grid(spec, bounds);
		gmpi::ui::Label one("One");
		gmpi::ui::Label two("Two");
		gmpi::ui::Label three("Three");

	builder.doChildLayout();

	ASSERT_EQ(1u, builder.childViews.size()); // one child, the grid.
	auto gridView = dynamic_cast<gmpi::ui::builder::Grid*>(builder.childViews[0].get());
	ASSERT_NE(gridView, nullptr);
	auto& views = gridView->childViews;

	ASSERT_EQ(3u, views.size());

	// 3 rows * 20 + 2 gaps * 5 = 70 total height, starting at top=10 -> bottom=80.
	EXPECT_FLOAT_EQ(80.0f, gridView->bounds.bottom);

	const auto b0 = views[0]->getBounds();
	const auto b1 = views[1]->getBounds();
	const auto b2 = views[2]->getBounds();

	EXPECT_FLOAT_EQ(10.0f, b0.top);
	EXPECT_FLOAT_EQ(30.0f, b0.bottom);

	EXPECT_FLOAT_EQ(35.0f, b1.top);
	EXPECT_FLOAT_EQ(55.0f, b1.bottom);

	EXPECT_FLOAT_EQ(60.0f, b2.top);
	EXPECT_FLOAT_EQ(80.0f, b2.bottom);
}

// test grid automatically sizing children (and stacking vertically)
TEST_F(TestUI, GridAutoSizeVertical)
{
	gmpi::ui::builder::ViewParent builder;
	gmpi::ui::builder::ThreadLocalCurrentBuilder = &builder;

	gmpi::ui::builder::Grid::Initializer spec;
	spec.gap = 3.0f;
	spec.auto_rows = 20.0f;
	spec.auto_columns = 30.0f;

	gmpi::drawing::Rect bounds{ 0.0f, 10.0f, 10.0f, 0.0f };

	gmpi::ui::Grid grid(spec, bounds);
		gmpi::ui::Label one("One");
		gmpi::ui::Label two("Two");

	builder.doChildLayout();

	ASSERT_EQ(1u, builder.childViews.size()); // one child, the grid.
	auto gridView = dynamic_cast<gmpi::ui::builder::Grid*>(builder.childViews[0].get());
	ASSERT_NE(gridView, nullptr);
	auto& views = gridView->childViews;

	ASSERT_EQ(2u, views.size());

	const auto b0 = views[0]->getBounds();
	const auto b1 = views[1]->getBounds();

	EXPECT_FLOAT_EQ(30.0f, getWidth(b0));
	EXPECT_FLOAT_EQ(20.0f, getHeight(b0));

	EXPECT_FLOAT_EQ(b0.bottom + spec.gap, b1.top);
	EXPECT_FLOAT_EQ(30.0f, getWidth(b1));
	EXPECT_FLOAT_EQ(20.0f, getHeight(b1));
}

// test grid automatically sizing children (and stacking horizontally)
TEST_F(TestUI, GridAutoSizeHorizontal)
{
	// create a blank form.
	gmpi::ui::builder::ViewParent builder;
	gmpi::ui::builder::ThreadLocalCurrentBuilder = &builder;

	// create a grid
	gmpi::ui::builder::Grid::Initializer spec;
	spec.gap = 3.0f;
	spec.auto_rows = 20.0f;
	spec.auto_columns = 30.0f;
	spec.auto_flow = gmpi::ui::builder::Grid::eAutoFlow::columns;

	gmpi::drawing::Rect bounds{ 0.0f, 10.0f, 10.0f, 0.0f };

	gmpi::ui::Grid grid(spec, bounds);
		gmpi::ui::Label one("One");
		gmpi::ui::Label two("Two");

	// layout grid and children.
	builder.doChildLayout();


	ASSERT_EQ(1u, builder.childViews.size()); // one child, the grid.
	auto gridView = dynamic_cast<gmpi::ui::builder::Grid*>(builder.childViews[0].get());
	ASSERT_NE(gridView, nullptr);
	auto& views = gridView->childViews;

	ASSERT_EQ(2u, views.size());

	const auto b0 = views[0]->getBounds();
	const auto b1 = views[1]->getBounds();

	EXPECT_FLOAT_EQ(30.0f, getWidth(b0));
	EXPECT_FLOAT_EQ(20.0f, getHeight(b0));

	EXPECT_FLOAT_EQ(b0.right + spec.gap, b1.left);
	EXPECT_FLOAT_EQ(30.0f, getWidth(b1));
	EXPECT_FLOAT_EQ(20.0f, getHeight(b1));
}

// tests that a grid inside a grid works as per CSS.
// the inner grid is larger than the outer, but the outer should ignore that.
TEST_F(TestUI, NestedGrids)
{
	// create a blank form.
	gmpi::ui::builder::ViewParent builder;
	gmpi::ui::builder::ThreadLocalCurrentBuilder = &builder;

	// create a grid
	gmpi::ui::builder::Grid::Initializer spec;
	spec.gap = 3.0f;
	spec.auto_rows = 20.0f;
	spec.auto_columns = 30.0f;
	spec.auto_flow = gmpi::ui::builder::Grid::eAutoFlow::rows;

	constexpr float innerGap = 1.0f;

	gmpi::drawing::Rect bounds{ 0.0f, 10.0f, 10.0f, 0.0f };

	{
		gmpi::ui::Grid grid(spec, bounds);
		{
			gmpi::ui::Grid inner_grid({ .gap = innerGap, .auto_rows = 13.f, .auto_columns = 50.0f, .auto_flow = gmpi::ui::builder::ViewParent::eAutoFlow::columns });
			{
				gmpi::ui::Label label1("top-left");
				gmpi::ui::Label label2("top-right");
			}
		}
	}

	// layout grid and children.
	builder.doChildLayout();


	ASSERT_EQ(1u, builder.childViews.size()); // one child, the grid.
	auto gridView = dynamic_cast<gmpi::ui::builder::Grid*>(builder.childViews[0].get());
	ASSERT_NE(gridView, nullptr);
	auto& views = gridView->childViews;

	ASSERT_EQ(1u, views.size());
	auto innerGridView = dynamic_cast<gmpi::ui::builder::Grid*>(views[0].get());
	auto& innerViews = innerGridView->childViews;
	ASSERT_EQ(2u, innerViews.size()); // one child, the grid.

	const auto b0 = views[0]->getBounds();
	const auto b1 = innerViews[0]->getBounds();
	const auto b2 = innerViews[1]->getBounds();

	// outer grid
	EXPECT_FLOAT_EQ(30.0f, getWidth(b0));
	EXPECT_FLOAT_EQ(20.0f, getHeight(b0));

	// inner items size by inner spec (50 x 13)
	EXPECT_FLOAT_EQ(50.0f, getWidth(b1));
	EXPECT_FLOAT_EQ(13.0f, getHeight(b1));

	// inner items stacked by inner spec (gap 1)
	EXPECT_FLOAT_EQ(b0.left, b1.left);
	EXPECT_FLOAT_EQ(b1.right + innerGap, b2.left);
}

// View::getDesired{Width,Height}() default reads bounds — leaf views can encode
// their intrinsic size in the constructor's bounds rect.
TEST_F(TestUI, ViewDesiredSizeDefaultsToBoundsDimensions)
{
	gmpi::ui::builder::ViewParent builder;
	gmpi::ui::builder::ThreadLocalCurrentBuilder = &builder;

	gmpi::ui::Label label("test", { 0.f, 5.f, 50.f, 23.f });

	EXPECT_FLOAT_EQ(50.0f, label.view->getDesiredWidth());
	EXPECT_FLOAT_EQ(18.0f, label.view->getDesiredHeight());
}

// auto_size() entries in column_heights resolve to each child's getDesiredHeight().
TEST_F(TestUI, GridColumnHeightsAutoSizeReadsChildBounds)
{
	gmpi::ui::builder::ViewParent builder;
	gmpi::ui::builder::ThreadLocalCurrentBuilder = &builder;

	{
		gmpi::ui::builder::Grid::Initializer spec;
		spec.gap = 5.0f;
		spec.column_heights = {
			gmpi::ui::builder::auto_size(),
			gmpi::ui::builder::auto_size(),
		};

		gmpi::drawing::Rect bounds{ 0.0f, 0.0f, 100.0f, 999.0f };

		gmpi::ui::Grid grid(spec, bounds);
			gmpi::ui::Label first("First",   { 0.f, 0.f, 0.f, 20.f });
			gmpi::ui::Label second("Second", { 0.f, 0.f, 0.f, 30.f });
	}

	builder.doChildLayout();

	auto gridView = dynamic_cast<gmpi::ui::builder::Grid*>(builder.childViews[0].get());
	auto& views = gridView->childViews;

	const auto b0 = views[0]->getBounds();
	const auto b1 = views[1]->getBounds();

	// first row sized to first child's desired height (20)
	EXPECT_FLOAT_EQ(0.0f, b0.top);
	EXPECT_FLOAT_EQ(20.0f, b0.bottom);

	// gap 5, then second row sized to second child's desired height (30)
	EXPECT_FLOAT_EQ(25.0f, b1.top);
	EXPECT_FLOAT_EQ(55.0f, b1.bottom);
}

// default_track_size = auto_size() makes every unspecified track ask its child.
// Lets a Grid host an arbitrary number of content-sized rows without enumerating
// column_heights upfront.
TEST_F(TestUI, GridDefaultTrackSizeAutoSize)
{
	gmpi::ui::builder::ViewParent builder;
	gmpi::ui::builder::ThreadLocalCurrentBuilder = &builder;

	{
		gmpi::ui::builder::Grid::Initializer spec;
		spec.gap = 5.0f;
		spec.default_track_size = gmpi::ui::builder::auto_size();

		gmpi::drawing::Rect bounds{ 0.0f, 0.0f, 100.0f, 999.0f };

		gmpi::ui::Grid grid(spec, bounds);
			gmpi::ui::Label one("One",     { 0.f, 0.f, 0.f, 10.f });
			gmpi::ui::Label two("Two",     { 0.f, 0.f, 0.f, 15.f });
			gmpi::ui::Label three("Three", { 0.f, 0.f, 0.f, 20.f });
	}

	builder.doChildLayout();

	auto gridView = dynamic_cast<gmpi::ui::builder::Grid*>(builder.childViews[0].get());
	auto& views = gridView->childViews;

	EXPECT_FLOAT_EQ(0.0f,  views[0]->getBounds().top);
	EXPECT_FLOAT_EQ(10.0f, views[0]->getBounds().bottom);

	EXPECT_FLOAT_EQ(15.0f, views[1]->getBounds().top);
	EXPECT_FLOAT_EQ(30.0f, views[1]->getBounds().bottom);

	EXPECT_FLOAT_EQ(35.0f, views[2]->getBounds().top);
	EXPECT_FLOAT_EQ(55.0f, views[2]->getBounds().bottom);
}

// When auto_size() points at a nested column-flow Grid, the outer reads
// the inner's getDesiredHeight() — which for column-flow returns the
// configured auto_rows (cross-axis) regardless of bounds height.
TEST_F(TestUI, GridAutoSizeReadsInnerColumnFlowGrid)
{
	gmpi::ui::builder::ViewParent builder;
	gmpi::ui::builder::ThreadLocalCurrentBuilder = &builder;

	{
		gmpi::ui::builder::Grid::Initializer outerSpec;
		outerSpec.gap = 3.0f;
		outerSpec.default_track_size = gmpi::ui::builder::auto_size();

		gmpi::drawing::Rect outerBounds{ 0.0f, 0.0f, 200.0f, 999.0f };
		gmpi::ui::Grid outer(outerSpec, outerBounds);

		auto makeRow = []() {
			gmpi::ui::builder::Grid::Initializer rowSpec;
			rowSpec.gap = 2.0f;
			rowSpec.auto_rows = 25.0f;
			rowSpec.auto_flow = gmpi::ui::builder::ViewParent::eAutoFlow::columns;
			rowSpec.column_widths = { gmpi::ui::builder::fr(1.0f), gmpi::ui::builder::fr(1.0f) };
			return rowSpec;
		};

		{
			gmpi::ui::Grid row1(makeRow());
				gmpi::ui::Label l1("a");
				gmpi::ui::Label l2("b");
		}
		{
			gmpi::ui::Grid row2(makeRow());
				gmpi::ui::Label l3("c");
				gmpi::ui::Label l4("d");
		}
	}

	builder.doChildLayout();

	auto outerView = dynamic_cast<gmpi::ui::builder::Grid*>(builder.childViews[0].get());
	auto& outerKids = outerView->childViews;

	// each inner (column-flow) Grid reports auto_rows (25) as its desired height
	EXPECT_FLOAT_EQ(0.0f,  outerKids[0]->getBounds().top);
	EXPECT_FLOAT_EQ(25.0f, outerKids[0]->getBounds().bottom);

	// gap of 3 between rows
	EXPECT_FLOAT_EQ(28.0f, outerKids[1]->getBounds().top);
	EXPECT_FLOAT_EQ(53.0f, outerKids[1]->getBounds().bottom);
}

// When auto_size() points at a nested row-flow Grid (e.g. a "toggle group" of
// N items), the outer reads inner's main-axis extent: count*auto_rows + gaps.
TEST_F(TestUI, GridAutoSizeReadsInnerRowFlowGrid)
{
	gmpi::ui::builder::ViewParent builder;
	gmpi::ui::builder::ThreadLocalCurrentBuilder = &builder;

	{
		gmpi::ui::builder::Grid::Initializer outerSpec;
		outerSpec.gap = 3.0f;
		outerSpec.default_track_size = gmpi::ui::builder::auto_size();

		gmpi::drawing::Rect outerBounds{ 0.0f, 0.0f, 200.0f, 999.0f };
		gmpi::ui::Grid outer(outerSpec, outerBounds);

		// vertical Grid: 3 children × 16 each, gap 2 → 48 + 4 = 52 total
		gmpi::ui::builder::Grid::Initializer innerSpec;
		innerSpec.gap = 2.0f;
		innerSpec.auto_rows = 16.0f;

		gmpi::ui::Grid toggleGroup(innerSpec);
			gmpi::ui::Label a("a");
			gmpi::ui::Label b("b");
			gmpi::ui::Label c("c");
	}

	builder.doChildLayout();

	auto outerView = dynamic_cast<gmpi::ui::builder::Grid*>(builder.childViews[0].get());
	auto& outerKids = outerView->childViews;

	EXPECT_FLOAT_EQ(0.0f,  outerKids[0]->getBounds().top);
	EXPECT_FLOAT_EQ(52.0f, outerKids[0]->getBounds().bottom);
}

} // namespace UnitTestSynthEdit

