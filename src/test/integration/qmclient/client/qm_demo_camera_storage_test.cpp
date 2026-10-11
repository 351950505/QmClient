#include <game/client/components/qmclient/demo_camera_file.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <memory>

namespace
{
	class CQmDemoCameraStorage : public ::testing::Test
	{
	protected:
		CTestInfo m_Info;
		std::unique_ptr<IStorage> m_pStorage = m_Info.CreateTestStorage();
		qm_demo_camera::SProject m_Project;
		const std::string m_Path = qm_demo_camera::SidecarPath("race.demo");

		void SetUp() override
		{
			m_Project.m_Enabled = true;
			ASSERT_GE(m_Project.m_Track.Insert({100.0, {{10.0f, 20.0f}, 1.5f}}), 0);
		}
	};
}

TEST_F(CQmDemoCameraStorage, ReopeningTheSidecarRestoresTheSavedCameraAndEnableState)
{
	ASSERT_TRUE(qm_demo_camera::Save(m_pStorage.get(), m_Path, m_Project));
	qm_demo_camera::SProject Reopened;
	ASSERT_EQ(qm_demo_camera::Load(m_pStorage.get(), m_Path, Reopened), qm_demo_camera::ELoadResult::LOADED);
	EXPECT_TRUE(Reopened.m_Enabled);
	const auto Sample = Reopened.m_Track.Sample(100.0);
	ASSERT_TRUE(Sample.has_value());
	EXPECT_EQ(Sample->m_Center, vec2(10.0f, 20.0f));
	EXPECT_FLOAT_EQ(Sample->m_Zoom, 1.5f);
	EXPECT_FALSE(m_pStorage->FileExists((m_Path + ".tmp").c_str(), IStorage::TYPE_SAVE));
}

TEST_F(CQmDemoCameraStorage, FailedWriteKeepsThePreviousFileAndCanBeRetried)
{
	ASSERT_TRUE(qm_demo_camera::Save(m_pStorage.get(), m_Path, m_Project));
	ASSERT_TRUE(m_pStorage->CreateFolder((m_Path + ".tmp").c_str(), IStorage::TYPE_SAVE));
	m_Project.m_Enabled = false;
	EXPECT_FALSE(qm_demo_camera::Save(m_pStorage.get(), m_Path, m_Project));
	qm_demo_camera::SProject Previous;
	ASSERT_EQ(qm_demo_camera::Load(m_pStorage.get(), m_Path, Previous), qm_demo_camera::ELoadResult::LOADED);
	EXPECT_TRUE(Previous.m_Enabled);
	ASSERT_TRUE(m_pStorage->RemoveFolder((m_Path + ".tmp").c_str(), IStorage::TYPE_SAVE));
	ASSERT_TRUE(qm_demo_camera::Save(m_pStorage.get(), m_Path, m_Project));
	ASSERT_EQ(qm_demo_camera::Load(m_pStorage.get(), m_Path, Previous), qm_demo_camera::ELoadResult::LOADED);
	EXPECT_FALSE(Previous.m_Enabled);
}

TEST_F(CQmDemoCameraStorage, MissingAndMalformedFilesAreDistinguishedWithoutLosingCurrentEdits)
{
	EXPECT_EQ(qm_demo_camera::Load(m_pStorage.get(), m_Path, m_Project), qm_demo_camera::ELoadResult::MISSING);
	IOHANDLE File = m_pStorage->OpenFile(m_Path.c_str(), IOFLAG_WRITE, IStorage::TYPE_SAVE);
	ASSERT_NE(File, nullptr);
	ASSERT_EQ(io_write(File, "broken", 6), 6U);
	ASSERT_EQ(io_close(File), 0);
	EXPECT_EQ(qm_demo_camera::Load(m_pStorage.get(), m_Path, m_Project), qm_demo_camera::ELoadResult::FAILED);
	EXPECT_TRUE(m_Project.m_Enabled);
	EXPECT_EQ(m_Project.m_Track.Frames().size(), 1U);
}

TEST_F(CQmDemoCameraStorage, ExportedCutReloadsWithTheSourceComposition)
{
	ASSERT_GE(m_Project.m_Track.Insert({200.0, {{110.0f, 220.0f}, 2.5f}}), 0);
	const auto Cut = m_Project.m_Track.Cut({{125, 175}});
	ASSERT_TRUE(Cut.has_value());
	ASSERT_TRUE(qm_demo_camera::Save(m_pStorage.get(), m_Path, {*Cut, true}));
	qm_demo_camera::SProject Reopened;
	ASSERT_EQ(qm_demo_camera::Load(m_pStorage.get(), m_Path, Reopened), qm_demo_camera::ELoadResult::LOADED);
	const auto ReopenedSample = Reopened.m_Track.Sample(150.0);
	const auto OriginalSample = m_Project.m_Track.Sample(150.0);
	ASSERT_TRUE(ReopenedSample.has_value());
	ASSERT_TRUE(OriginalSample.has_value());
	EXPECT_NEAR(ReopenedSample->m_Center.x, OriginalSample->m_Center.x, 0.0001f);
}

TEST_F(CQmDemoCameraStorage, SidecarIsResolvedBesideTheSelectedDemo)
{
	IOHANDLE File = m_pStorage->OpenFile("race.demo", IOFLAG_WRITE, IStorage::TYPE_SAVE);
	ASSERT_NE(File, nullptr);
	ASSERT_EQ(io_close(File), 0);
	const std::string Path = qm_demo_camera::ResolveSidecar(m_pStorage.get(), "race.demo", IStorage::TYPE_SAVE);
	ASSERT_FALSE(Path.empty());
	EXPECT_FALSE(fs_is_relative_path(Path.c_str()));
	ASSERT_TRUE(qm_demo_camera::Save(m_pStorage.get(), Path, m_Project));
	qm_demo_camera::SProject Restored;
	ASSERT_EQ(qm_demo_camera::Load(m_pStorage.get(), m_Path, Restored), qm_demo_camera::ELoadResult::LOADED);
	const auto Sample = Restored.m_Track.Sample(100.0);
	ASSERT_TRUE(Sample.has_value());
	EXPECT_EQ(Sample->m_Center, vec2(10.0f, 20.0f));
	EXPECT_TRUE(qm_demo_camera::ResolveSidecar(m_pStorage.get(), "missing.demo", IStorage::TYPE_SAVE).empty());
}
