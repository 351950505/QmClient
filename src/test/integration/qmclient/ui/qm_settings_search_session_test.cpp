#include <game/client/QmUi/SettingsSearchSession.h>

#include <test/support/qm_real_ui_fixture.h>
#include <test/support/qm_ui_test_client.h>
#include <test/support/qm_ui_test_input.h>

namespace
{
	class QmSettingsSearchSession : public qm_ui_test::CRealUiFixture
	{
	protected:
		qm_ui_test::CTestInput m_Input;
		qm_ui_test::CTestUiClient m_Client;
		IInput *UiInput() override { return &m_Input; }
		IClient *UiClient() override { return &m_Client; }
	};
}

TEST_F(QmSettingsSearchSession, LeavingAndReopeningPreservesQuery)
{
	CLineInputBuffered<128> Query;
	Query.Set("inp_mousesens");
	m_Ui.SetActiveItem(&Query);
	Query.Activate(EInputPriority::UI);
	ReleaseSettingsSearchInput(&m_Ui, &Query);
	EXPECT_NE(m_Ui.ActiveItem(), &Query);
	EXPECT_FALSE(Query.IsActive());
	EXPECT_STREQ(Query.GetString(), "inp_mousesens");
	// 第二次离页覆盖已经没有 UI 编辑权的路径。
	ReleaseSettingsSearchInput(&m_Ui, &Query);
	Query.Activate(EInputPriority::UI);
	EXPECT_STREQ(Query.GetString(), "inp_mousesens");
	Query.Deactivate();
}

TEST_F(QmSettingsSearchSession, ExplicitClearAndFreshSessionStartWithEmptyQuery)
{
	CLineInputBuffered<128> Query;
	Query.Set("snd_volume");
	ReleaseSettingsSearchInput(&m_Ui, &Query);
	EXPECT_FALSE(Query.IsEmpty());
	Query.Clear();
	EXPECT_TRUE(Query.IsEmpty());
	CLineInputBuffered<128> RestartedQuery;
	EXPECT_TRUE(RestartedQuery.IsEmpty());
}
