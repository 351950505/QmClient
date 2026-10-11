#include <game/client/components/qmclient/demo_camera_file.h>

#include <gtest/gtest.h>

using namespace qm_demo_camera;

TEST(QmDemoCameraFile, RoundTripPreservesEnabledStateAspectAndFractionalKeyframeTimes)
{
	SProject Source;
	Source.m_Enabled = true;
	ASSERT_TRUE(Source.m_Track.SetAspect(1.5f));
	ASSERT_GE(Source.m_Track.Insert({101.125, {{10.0f, -20.0f}, 1.25f}, ETransition::LINEAR}), 0);
	ASSERT_GE(Source.m_Track.Insert({201.5, {{30.0f, 40.0f}, 2.0f}}), 0);
	SProject Restored;
	ASSERT_TRUE(Decode(Encode(Source), Restored));
	EXPECT_TRUE(Restored.m_Enabled);
	EXPECT_FLOAT_EQ(Restored.m_Track.Aspect(), 1.5f);
	ASSERT_EQ(Restored.m_Track.Frames().size(), 2U);
	EXPECT_DOUBLE_EQ(Restored.m_Track.Frames()[0].m_Tick, 101.125);
	const auto RestoredSample = Restored.m_Track.Sample(151.0);
	const auto SourceSample = Source.m_Track.Sample(151.0);
	ASSERT_TRUE(RestoredSample.has_value());
	ASSERT_TRUE(SourceSample.has_value());
	EXPECT_EQ(RestoredSample->m_Center, SourceSample->m_Center);
	EXPECT_FLOAT_EQ(RestoredSample->m_Zoom, SourceSample->m_Zoom);
}

TEST(QmDemoCameraFile, CroppedEasingSurvivesSavingAndReloading)
{
	SProject Source;
	ASSERT_GE(Source.m_Track.Insert({100.0, {{0.0f, 0.0f}, 1.0f}}), 0);
	ASSERT_GE(Source.m_Track.Insert({200.0, {{100.0f, 0.0f}, 2.0f}}), 0);
	const auto Cut = Source.m_Track.Cut({{120, 180}});
	ASSERT_TRUE(Cut.has_value());
	SProject Restored;
	ASSERT_TRUE(Decode(Encode({*Cut, true}), Restored));
	const auto RestoredSample = Restored.m_Track.Sample(140.0);
	const auto SourceSample = Source.m_Track.Sample(140.0);
	ASSERT_TRUE(RestoredSample.has_value());
	ASSERT_TRUE(SourceSample.has_value());
	EXPECT_NEAR(RestoredSample->m_Center.x, SourceSample->m_Center.x, 0.0001f);
}

TEST(QmDemoCameraFile, InvalidOrUnsupportedFilesDoNotReplaceTheCurrentProject)
{
	SProject Project;
	ASSERT_GE(Project.m_Track.Insert({10.0, {{5.0f, 6.0f}, 1.0f}}), 0);
	for(const char *pJson : {
		    "invalid", "[]", "{}",
		    "{\"version\":2,\"enabled\":true,\"aspect\":1.5,\"keyframes\":[]}",
		    "{\"version\":1,\"enabled\":true,\"aspect\":0,\"keyframes\":[]}",
		    "{\"version\":1,\"enabled\":true,\"aspect\":1.5,\"keyframes\":[{}]}"})
	{
		SCOPED_TRACE(pJson);
		EXPECT_FALSE(Decode(pJson, Project));
		ASSERT_EQ(Project.m_Track.Frames().size(), 1U);
		const auto Sample = Project.m_Track.Sample(10.0);
		ASSERT_TRUE(Sample.has_value());
		EXPECT_EQ(Sample->m_Center, vec2(5.0f, 6.0f));
	}
	EXPECT_FALSE(Decode(std::string(512 * 1024 + 1, ' '), Project));
}

TEST(QmDemoCameraFile, EmptyProjectPersistsAUserDisabledPath)
{
	SProject Restored;
	Restored.m_Enabled = true;
	ASSERT_TRUE(Decode(Encode({}), Restored));
	EXPECT_TRUE(Restored.m_Track.Empty());
	EXPECT_FALSE(Restored.m_Enabled);
	EXPECT_TRUE(SidecarPath("").empty());
	EXPECT_EQ(SidecarPath("demos/race.demo"), "demos/race.demo.qm-camera.json");
}

TEST(QmDemoCameraFile, DuplicateTimesAndInvalidZoomAreRejected)
{
	SProject Source;
	ASSERT_GE(Source.m_Track.Insert({10.0, {{5.0f, 6.0f}, 1.0f}}), 0);
	ASSERT_GE(Source.m_Track.Insert({20.0, {{7.0f, 8.0f}, 2.0f}}), 0);
	std::string Json = Encode(Source);
	const size_t Tick = Json.find("\"tick\":20");
	ASSERT_NE(Tick, std::string::npos);
	Json.replace(Tick, 9, "\"tick\":10");
	SProject Restored;
	EXPECT_FALSE(Decode(Json, Restored));
	Json = Encode(Source);
	const size_t Zoom = Json.find("\"zoom\":1");
	ASSERT_NE(Zoom, std::string::npos);
	Json.replace(Zoom, 8, "\"zoom\":0");
	EXPECT_FALSE(Decode(Json, Restored));
}
