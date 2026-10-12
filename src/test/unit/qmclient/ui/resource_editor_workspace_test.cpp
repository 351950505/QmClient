#include <game/client/QmUi/ResourceEditorWorkspace.h>

#include <gtest/gtest.h>

TEST(ResourceEditorWorkspace, SwitchingEditorsKeepsTheOriginalReturnDestination)
{
	qm_resource_editor::CWorkspace Workspace;
	Workspace.Enter(12, 0);
	EXPECT_TRUE(Workspace.Select(1));
	EXPECT_EQ(Workspace.Selected(), qm_resource_editor::EWorkspace::AUDIO);
	EXPECT_EQ(Workspace.ReturnPage(), 12);
	Workspace.Enter(99, 0);
	EXPECT_EQ(Workspace.Selected(), qm_resource_editor::EWorkspace::IMAGE);
	EXPECT_EQ(Workspace.ReturnPage(), 12);
}

TEST(ResourceEditorWorkspace, ReopeningResumesTheEditorAndReturnsToTheNewCaller)
{
	qm_resource_editor::CWorkspace Workspace;
	Workspace.Enter(12, 1);
	Workspace.Suspend();
	EXPECT_FALSE(Workspace.Visible());
	EXPECT_EQ(Workspace.Selected(), qm_resource_editor::EWorkspace::AUDIO);
	Workspace.Enter(18);
	EXPECT_TRUE(Workspace.Visible());
	EXPECT_EQ(Workspace.Selected(), qm_resource_editor::EWorkspace::AUDIO);
	EXPECT_EQ(Workspace.ReturnPage(), 18);
}

TEST(ResourceEditorWorkspace, InvalidWorkspaceDoesNotReplaceTheCurrentEditor)
{
	qm_resource_editor::CWorkspace Workspace;
	Workspace.Enter(12, 1);
	EXPECT_FALSE(Workspace.Select(-1));
	EXPECT_FALSE(Workspace.Select(2));
	EXPECT_EQ(Workspace.Selected(), qm_resource_editor::EWorkspace::AUDIO);
}

TEST(ResourceEditorWorkspace, ResumingAfterMenuHideKeepsTheOriginalReturnDestination)
{
	qm_resource_editor::CWorkspace Workspace;
	Workspace.Enter(12, 1);
	Workspace.Suspend();
	Workspace.Enter(Workspace.ReturnPage());
	EXPECT_TRUE(Workspace.Visible());
	EXPECT_EQ(Workspace.ReturnPage(), 12);
	EXPECT_EQ(Workspace.Selected(), qm_resource_editor::EWorkspace::AUDIO);
}
