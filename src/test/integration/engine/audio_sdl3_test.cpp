// 使用真实 SDL 音频转换流，验证应用混音入口，不访问硬件或用户配置。
#include <engine/client/audio_sdl3.h>

#include <gtest/gtest.h>

#include <array>
#include <climits>
#include <memory>
#include <vector>

namespace
{
	class Sdl3AudioStream : public ::testing::Test
	{
	protected:
		std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)> m_pStream{nullptr, SDL_DestroyAudioStream};
		void SetUp() override
		{
			SDL_AudioSpec Spec{};
			Spec.format = SDL_AUDIO_S16;
			Spec.channels = 2;
			Spec.freq = 48000;
			m_pStream.reset(SDL_CreateAudioStream(&Spec, &Spec));
			ASSERT_NE(m_pStream, nullptr) << SDL_GetError();
		}
	};
}

TEST_F(Sdl3AudioStream, LargeDeviceRequestUsesBoundedChunksAndPreservesSamples)
{
	std::array<int16_t, 8> aBuffer{};
	std::vector<int> vChunkFrames;
	int Sample = 0;
	ASSERT_TRUE(FillSdlAudioStream(m_pStream.get(), 40, 2, aBuffer.data(), 4, [&](int16_t *pBuffer, int Frames) {
		vChunkFrames.push_back(Frames);
		for(int i = 0; i < Frames * 2; ++i)
			pBuffer[i] = ++Sample;
	}));
	EXPECT_EQ(vChunkFrames, (std::vector<int>{4, 4, 2}));
	ASSERT_EQ(SDL_GetAudioStreamAvailable(m_pStream.get()), 40);
	std::array<int16_t, 20> aOutput{};
	ASSERT_EQ(SDL_GetAudioStreamData(m_pStream.get(), aOutput.data(), sizeof(aOutput)), sizeof(aOutput));
	for(size_t i = 0; i < aOutput.size(); ++i)
		EXPECT_EQ(aOutput[i], i + 1);
}

TEST_F(Sdl3AudioStream, PartialFrameRequestSuppliesACompleteInterleavedFrame)
{
	std::array<int16_t, 2> aBuffer{};
	int Calls = 0;
	ASSERT_TRUE(FillSdlAudioStream(m_pStream.get(), 1, 2, aBuffer.data(), 1, [&](int16_t *pBuffer, int Frames) {
		++Calls;
		EXPECT_EQ(Frames, 1);
		pBuffer[0] = 123;
		pBuffer[1] = -123;
	}));
	EXPECT_EQ(Calls, 1);
	std::array<int16_t, 2> aOutput{};
	ASSERT_EQ(SDL_GetAudioStreamData(m_pStream.get(), aOutput.data(), sizeof(aOutput)), sizeof(aOutput));
	EXPECT_EQ(aOutput, (std::array<int16_t, 2>{123, -123}));
}

TEST_F(Sdl3AudioStream, EmptyRequestsDoNotMixOrAppendData)
{
	std::array<int16_t, 2> aBuffer{};
	for(int Bytes : {0, -1})
		EXPECT_TRUE(FillSdlAudioStream(m_pStream.get(), Bytes, 2, aBuffer.data(), 1, [](int16_t *, int) { FAIL() << "empty request mixed audio"; }));
	EXPECT_EQ(SDL_GetAudioStreamAvailable(m_pStream.get()), 0);
}

TEST_F(Sdl3AudioStream, InvalidBufferAndChannelLimitsFailWithoutCallingMixer)
{
	std::array<int16_t, 2> aBuffer{};
	const auto NeverMix = [](int16_t *, int) { FAIL() << "invalid buffer mixed audio"; };
	EXPECT_FALSE(FillSdlAudioStream(m_pStream.get(), 4, 2, aBuffer.data(), 0, NeverMix));
	EXPECT_FALSE(FillSdlAudioStream(m_pStream.get(), 4, 0, aBuffer.data(), 1, NeverMix));
	EXPECT_FALSE(FillSdlAudioStream(m_pStream.get(), 4, 3, aBuffer.data(), 1, NeverMix));
	EXPECT_FALSE(FillSdlAudioStream(m_pStream.get(), 4, 2, aBuffer.data(), INT_MAX, NeverMix));
	EXPECT_FALSE(FillSdlAudioStream(m_pStream.get(), 4, 2, nullptr, 1, NeverMix));
	EXPECT_FALSE(FillSdlAudioStream(nullptr, 4, 2, aBuffer.data(), 1, NeverMix));
	EXPECT_EQ(SDL_GetAudioStreamAvailable(m_pStream.get()), 0);
}

TEST(Sdl3VoiceAudioStream, TwentyMillisecondMonoFrameKeepsDurationAcrossDeviceResampling)
{
	SDL_AudioSpec Application{SDL_AUDIO_S16, 1, 48000};
	SDL_AudioSpec Device{SDL_AUDIO_S16, 1, 24000};
	std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)> pStream(SDL_CreateAudioStream(&Application, &Device), SDL_DestroyAudioStream);
	ASSERT_NE(pStream, nullptr) << SDL_GetError();
	std::array<int16_t, 960> aFrame{};
	ASSERT_TRUE(FillSdlAudioStream(pStream.get(), sizeof(aFrame), 1, aFrame.data(), aFrame.size(), [](int16_t *pBuffer, int Frames) {
		std::fill_n(pBuffer, Frames, 1000);
	}));
	ASSERT_TRUE(SDL_FlushAudioStream(pStream.get()));
	ASSERT_EQ(SDL_GetAudioStreamAvailable(pStream.get()), 480 * sizeof(int16_t));
	std::array<int16_t, 480> aDeviceFrame{};
	ASSERT_EQ(SDL_GetAudioStreamData(pStream.get(), aDeviceFrame.data(), sizeof(aDeviceFrame)), sizeof(aDeviceFrame));
	EXPECT_NEAR(aDeviceFrame[240], 1000, 2);
	EXPECT_EQ(SDL_GetAudioStreamAvailable(pStream.get()), 0);
}
