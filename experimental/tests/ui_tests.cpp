#include <gtest/gtest.h>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <cstdio>

#include "experimental/forms.h"

namespace UnitTestSynthEdit
{

class TestEnvironment : public ::testing::Test
{
};

TEST_F(TestEnvironment, GridDestructorMovesChildrenToParentAndStacksEvenly)
{
	std::vector<std::unique_ptr<gmpi::ui::builder::View>> parentViews;

	{
		gmpi::ui::builder::Builder builder(parentViews);

		gmpi::ui::Grid::Initializer spec;
		spec.gap = 10.0f;

		gmpi::ui::Grid grid(spec);
		grid.bounds = { 0.0f, 0.0f, 100.0f, 100.0f };

		gmpi::ui::Label first("First");
		gmpi::ui::Label second("Second");
	}

	ASSERT_EQ(2u, parentViews.size());

	const auto b0 = parentViews[0]->getBounds();
	const auto b1 = parentViews[1]->getBounds();

	// rowHeight = (100 - 10) / 2 = 45
	EXPECT_FLOAT_EQ(0.0f, b0.top);
	EXPECT_FLOAT_EQ(45.0f, b0.bottom);

	EXPECT_FLOAT_EQ(55.0f, b1.top);
	EXPECT_FLOAT_EQ(100.0f, b1.bottom);
}

TEST_F(TestEnvironment, GridAutoRowsUsesFixedHeightAndResizesBottomToFitChildren)
{
	std::vector<std::unique_ptr<gmpi::ui::builder::View>> parentViews;
	gmpi::ui::Grid::Initializer spec;
	spec.gap = 5.0f;
	spec.auto_rows = 20.0f;

	gmpi::ui::builder::Builder builder(parentViews);
	gmpi::ui::Grid grid(spec);
	grid.bounds = { 0.0f, 10.0f, 200.0f, 999.0f };

	gmpi::ui::Label one("One");
	gmpi::ui::Label two("Two");
	gmpi::ui::Label three("Three");

	grid.doLayout();

	ASSERT_TRUE(grid.layoutDone);
	ASSERT_EQ(3u, parentViews.size());

	// 3 rows * 20 + 2 gaps * 5 = 70 total height, starting at top=10 -> bottom=80.
	EXPECT_FLOAT_EQ(80.0f, grid.bounds.bottom);

	const auto b0 = parentViews[0]->getBounds();
	const auto b1 = parentViews[1]->getBounds();
	const auto b2 = parentViews[2]->getBounds();

	EXPECT_FLOAT_EQ(10.0f, b0.top);
	EXPECT_FLOAT_EQ(30.0f, b0.bottom);

	EXPECT_FLOAT_EQ(35.0f, b1.top);
	EXPECT_FLOAT_EQ(55.0f, b1.bottom);

	EXPECT_FLOAT_EQ(60.0f, b2.top);
	EXPECT_FLOAT_EQ(80.0f, b2.bottom);
}

} // namespace UnitTestSynthEdit

