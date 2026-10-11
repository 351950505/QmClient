// 真实 UI 元素与文字引擎的资源生命周期协作；仅替换 GPU 设备提交。
#include <test/support/qm_real_ui_fixture.h>

#include <memory>
#include <unordered_map>

namespace
{
	class QmUiElementLifetime : public qm_ui_test::CRealUiFixture
	{
	protected:
		qm_ui_test::CRealUiTestGraphics &Graphics()
		{
			return *static_cast<qm_ui_test::CRealUiTestGraphics *>(m_Ui.Graphics());
		}

		void Populate(CUIElement &Element, const char *pText = "036")
		{
			const CUIRect Rect{0, 0, 96, 18};
			SLabelProperties Props;
			Props.m_MaxWidth = Rect.w;
			Props.m_StopAtEnd = true;
			Props.m_EllipsisAtEnd = true;
			m_Ui.DoLabelStreamed(*Element.Rect(0), &Rect, pText, 12, TEXTALIGN_ML, Props);
		}
	};
}

TEST_F(QmUiElementLifetime, DestroyingElementReturnsItsTextBuffers)
{
	const auto InitialContainers = Graphics().m_Containers.size();
	const auto InitialBuffers = Graphics().m_Buffers.size();
	std::weak_ptr<STextContainerUsages> Usage;
	{
		CUIElement Element;
		Element.Init(&m_Ui, 1);
		Populate(Element);
		ASSERT_TRUE(Element.Rect(0)->m_UITextContainer.Valid());
		Usage = Element.Rect(0)->m_UITextContainer.m_UseCount;
		ASSERT_GT(Graphics().m_Containers.size(), InitialContainers);
	}

	ASSERT_TRUE(Usage.expired());
	EXPECT_EQ(Graphics().m_Containers.size(), InitialContainers);
	EXPECT_EQ(Graphics().m_Buffers.size(), InitialBuffers);
	m_Ui.TextRender()->OnPreWindowResize();
}

TEST_F(QmUiElementLifetime, EvictingCachedElementsAllowsResizeAndRecreation)
{
	std::unordered_map<int, std::unique_ptr<CUIElement>> Cache;
	for(int Id = 0; Id < 2; ++Id)
	{
		auto pElement = std::make_unique<CUIElement>();
		pElement->Init(&m_Ui, 1);
		Populate(*pElement);
		ASSERT_TRUE(pElement->Rect(0)->m_UITextContainer.Valid());
		Cache.emplace(Id, std::move(pElement));
	}
	const std::weak_ptr<STextContainerUsages> EvictedUsage = Cache.at(0)->Rect(0)->m_UITextContainer.m_UseCount;
	Cache.erase(0);
	ASSERT_TRUE(EvictedUsage.expired());
	EXPECT_TRUE(Cache.at(1)->Rect(0)->m_UITextContainer.Valid());

	m_Ui.TextRender()->OnPreWindowResize();
	m_Ui.OnWindowResize();
	m_Ui.TextRender()->OnWindowResize();
	EXPECT_FALSE(Cache.at(1)->Rect(0)->m_UITextContainer.Valid());
	Populate(*Cache.at(1), "Recreated");
	ASSERT_TRUE(Cache.at(1)->Rect(0)->m_UITextContainer.Valid());
	const std::weak_ptr<STextContainerUsages> RecreatedUsage = Cache.at(1)->Rect(0)->m_UITextContainer.m_UseCount;
	Cache.clear();
	ASSERT_TRUE(RecreatedUsage.expired());
	m_Ui.TextRender()->OnPreWindowResize();
}

TEST_F(QmUiElementLifetime, DestroyingResetElementKeepsReusedTextSlotAlive)
{
	auto pOldElement = std::make_unique<CUIElement>();
	pOldElement->Init(&m_Ui, 1);
	Populate(*pOldElement);
	ASSERT_TRUE(pOldElement->Rect(0)->m_UITextContainer.Valid());
	const int OldSlot = pOldElement->Rect(0)->m_UITextContainer.m_Index;
	m_Ui.ResetUIElement(*pOldElement);
	m_Ui.ResetUIElement(*pOldElement);

	CUIElement NewElement;
	NewElement.Init(&m_Ui, 1);
	Populate(NewElement, "Replacement");
	ASSERT_TRUE(NewElement.Rect(0)->m_UITextContainer.Valid());
	ASSERT_EQ(NewElement.Rect(0)->m_UITextContainer.m_Index, OldSlot);
	const auto LiveContainers = Graphics().m_Containers.size();
	pOldElement.reset();
	EXPECT_TRUE(NewElement.Rect(0)->m_UITextContainer.Valid());
	EXPECT_EQ(Graphics().m_Containers.size(), LiveContainers);
	m_Ui.TextRender()->OnPreWindowResize();
}

TEST_F(QmUiElementLifetime, ElementCanBeDestroyedAfterUiShutdown)
{
	auto pElement = std::make_unique<CUIElement>();
	pElement->Init(&m_Ui, 1);
	Populate(*pElement);
	ASSERT_TRUE(pElement->Rect(0)->m_UITextContainer.Valid());
	STextContainerIndex PreviousContainer = pElement->Rect(0)->m_UITextContainer;

	m_Ui.OnShutdown();
	EXPECT_FALSE(PreviousContainer.Valid());
	EXPECT_EQ(m_Ui.Graphics(), nullptr);
	EXPECT_EQ(m_Ui.TextRender(), nullptr);
	pElement.reset();
	EXPECT_FALSE(PreviousContainer.Valid());
}
