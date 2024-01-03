/******************************************************************************
 *
 * Copyright (C) 1997-2015 by Dimitri van Heesch.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation under the terms of the GNU General Public License is hereby
 * granted. No representations are made about the suitability of this software
 * for any purpose. It is provided "as is" without express or implied warranty.
 * See the GNU General Public License for more details.
 *
 * Documents produced by Doxygen are derivative works derived from the
 * input used in their production; they are not affected by this license.
 *
 */

#include "pagedef.h"
#include "groupdef.h"
#include "docparser.h"
#include "config.h"
#include "util.h"
#include "outputlist.h"
#include "doxygen.h"
#include "language.h"
#include "namespacedef.h"
#include "reflist.h"
#include "definitionimpl.h"

//------------------------------------------------------------------------------------------

class PageDefImpl : public DefinitionMixin<PageDef>
{
  public:
    PageDefImpl(const QCString &f,int l,const QCString &n,const QCString &d,const QCString &t);
    virtual ~PageDefImpl();

    virtual void setFileName(const QCString &name) override;
    virtual void setLocalToc(const LocalToc &tl) override;
    virtual void setShowLineNo(bool) override;
    virtual DefType definitionType() const override { return TypePage; }
    virtual CodeSymbolType codeSymbolType() const override { return CodeSymbolType::Default; }
    virtual bool isLinkableInProject() const override { return /*hasDocumentation() &&*/ !isReference(); }
    virtual bool isLinkable() const override { return isLinkableInProject() || isReference(); }
    virtual QCString getOutputFileBase() const override;
    virtual QCString anchor() const override { return QCString(); }
    virtual void findSectionsInDocumentation() override;
    virtual QCString title() const override { return m_title; }
    virtual const GroupDef * getGroupDef() const override;
    virtual const PageLinkedRefMap &getSubPages() const override { return m_subPages; }
    virtual void addInnerCompound(Definition *d) override;
    virtual bool visibleInIndex() const override;
    virtual bool documentedPage() const override;
    virtual bool hasSubPages() const override;
    virtual bool hasParentPage() const override;
    virtual bool hasTitle() const override;
    virtual LocalToc localToc() const override { return m_localToc; }
    virtual void setPageScope(Definition *d) override { m_pageScope = d; }
    virtual Definition *getPageScope() const override { return m_pageScope; }
    virtual QCString displayName(bool=TRUE) const override { return hasTitle() ? m_title : DefinitionMixin::name(); }
    virtual bool showLineNo() const override;
    virtual void setTitle(const QCString &title) override;
    virtual void writeDocumentation(OutputList &ol) override;
    virtual void writeTagFile(TextStream &) override;
    virtual void setNestingLevel(int l) override;
    virtual void writePageDocumentation(OutputList &ol) const override;

  private:
    QCString m_fileName;
    QCString m_title;
    PageLinkedRefMap m_subPages;                 // list of pages in the group
    Definition *m_pageScope;
    int m_nestingLevel;
    LocalToc m_localToc;
    bool m_showLineNo;
};

std::unique_ptr<PageDef> createPageDef(const QCString &f,int l,const QCString &n,const QCString &d,const QCString &t)
{
  return std::make_unique<PageDefImpl>(f,l,n,d,t);
}

//------------------------------------------------------------------------------------------

PageDefImpl::PageDefImpl(const QCString &f,int l,const QCString &n,
                 const QCString &d,const QCString &t)
 : DefinitionMixin(f,l,1,n), m_title(!t.isEmpty() ? t : n)
{
  setDocumentation(d,f,l);
  m_pageScope = 0;
  m_nestingLevel = 0;
  m_fileName = ::convertNameToFile(n,FALSE,TRUE);
  m_showLineNo = FALSE;
}

PageDefImpl::~PageDefImpl()
{
}

void PageDefImpl::findSectionsInDocumentation()
{
  docFindSections(briefDescription(),this,docFile());
  docFindSections(documentation(),this,docFile());
}

const GroupDef *PageDefImpl::getGroupDef() const
{
  return !partOfGroups().empty() ? partOfGroups().front() : 0;
}

QCString PageDefImpl::getOutputFileBase() const
{
  if (getGroupDef())
    return getGroupDef()->getOutputFileBase();
  else
    return m_fileName;
}

void PageDefImpl::setFileName(const QCString &name)
{
  m_fileName = name;
}

void PageDefImpl::addInnerCompound(Definition *def)
{
  if (def->definitionType()==Definition::TypePage)
  {
    PageDef *pd = const_cast<PageDef*>(toPageDef(def));
    if (pd)
    {
      m_subPages.add(pd->name(),pd);
      pd->setOuterScope(this);
      if (this==Doxygen::mainPage.get())
      {
        pd->setNestingLevel(m_nestingLevel);
      }
      else
      {
        pd->setNestingLevel(m_nestingLevel+1);
      }
    }
  }
}

bool PageDefImpl::hasParentPage() const
{
  return getOuterScope() &&
         getOuterScope()->definitionType()==Definition::TypePage;
}

void PageDefImpl::writeTagFile(TextStream &tagFile)
{
  bool found = name()=="citelist";
  for (RefListManager::Ptr &rl : RefListManager::instance())
  {
    if (rl->listName()==name())
    {
      found=TRUE;
      break;
    }
  }
  if (!found) // not one of the generated related pages
  {
    QCString fn = getOutputFileBase();
    addHtmlExtensionIfMissing(fn);
    tagFile << "  <compound kind=\"page\">\n";
    tagFile << "    <name>" << name() << "</name>\n";
    tagFile << "    <title>" << convertToXML(title()) << "</title>\n";
    tagFile << "    <filename>" << fn << "</filename>\n";
    for (const auto &subPage : m_subPages)
    {
      QCString sfn = subPage->getOutputFileBase();
      addHtmlExtensionIfMissing(sfn);
      tagFile << "    <subpage>" << sfn << "</subpage>\n";
    }
    writeDocAnchorsToTagFile(tagFile);
    tagFile << "  </compound>\n";
  }
}

void PageDefImpl::writeDocumentation(OutputList &ol)
{
  bool generateTreeView = Config_getBool(GENERATE_TREEVIEW);
  int hierarchyLevel = -1; // Pages start at the root
  PageDef *pd = this;
  while (pd->hasParentPage())
  {
    pd = (PageDef *)pd->getOuterScope();
    ++hierarchyLevel;
  }

  //outputList->disable(OutputType::Man);
  QCString pageName,manPageName;
  pageName    = escapeCharsInString(name(),FALSE,TRUE);
  manPageName = escapeCharsInString(name(),TRUE,TRUE);

  //printf("PageDefImpl::writeDocumentation: %s\n",getOutputFileBase().data());

  ol.pushGeneratorState();
  //1.{

  ol.pushGeneratorState();
  //2.{
  ol.disableAllBut(OutputType::Man);
  startFile(ol,getOutputFileBase(),manPageName,title(),HighlightedItem::Pages,!generateTreeView,
            QCString() /* altSidebarName */, hierarchyLevel);
  ol.enableAll();
  ol.disable(OutputType::Man);
  startFile(ol,getOutputFileBase(),pageName,title(),HighlightedItem::Pages,!generateTreeView,
            QCString() /* altSidebarName */, hierarchyLevel);
  ol.popGeneratorState();
  //2.}

  if (!generateTreeView)
  {
    if (getOuterScope()!=Doxygen::globalScope && !Config_getBool(DISABLE_INDEX))
    {
      DefinitionMutable *outerScope = toDefinitionMutable(getOuterScope());
      if (outerScope)
      {
        outerScope->writeNavigationPath(ol);
      }
    }
    ol.endQuickIndices();
  }
  const SectionInfo *si=SectionManager::instance().find(name());

  // save old generator state and write title only to Man generator
  ol.pushGeneratorState();
  //2.{
  ol.disableAllBut(OutputType::Man);
  ol.startTitleHead(manPageName);
  ol.endTitleHead(manPageName, manPageName);
  if (si)
  {
    ol.writeString(" - ");

    if (si->title() != manPageName)
    {
      ol.generateDoc(docFile(),getStartBodyLine(),this,0,si->title(),TRUE,FALSE,
                     QCString(),TRUE,FALSE,Config_getBool(MARKDOWN_SUPPORT));
      ol.endSection(si->label(),si->type());
    }
  }
  ol.popGeneratorState();
  //2.}

  ol.pushGeneratorState();
  //2.{
  ol.disable(OutputType::Man);
  QCString title;
  if (this == Doxygen::mainPage.get() && !hasTitle())
    title = theTranslator->trMainPage();
  else
    title = m_title;

  if (!title.isEmpty() && !name().isEmpty() && si!=0)
  {
    ol.startPageDoc(si->title());
    //ol.startSection(si->label,si->title,si->type);
    ol.startHeaderSection();
    ol.startTitleHead(getOutputFileBase());
    ol.parseText(title);
    ol.endTitleHead(getOutputFileBase(),title);
    ol.endHeaderSection();

    /*ol.generateDoc(docFile(),getStartBodyLine(),this,0,si->title(),TRUE,FALSE,
                   QCString(),TRUE,FALSE,Config_getBool(MARKDOWN_SUPPORT));*/
  }
  else
    ol.startPageDoc("");
  ol.popGeneratorState();
  //2.}

  ol.startContents();
  if ((m_localToc.isHtmlEnabled() || m_localToc.isLatexEnabled() || m_localToc.isDocbookEnabled())
    && hasSections())
  {
    writeToc(ol, m_localToc);
  }

  writePageDocumentation(ol);
  ol.endContents();
  ol.endPageDoc();

  if (generateTreeView && getOuterScope()!=Doxygen::globalScope && !Config_getBool(DISABLE_INDEX))
  {
    endFileWithNavPath(ol,getOuterScope());
  }
  else
  {
    endFile(ol,FALSE,TRUE);
  }

  ol.popGeneratorState();
  //1.}
}

void PageDefImpl::writePageDocumentation(OutputList &ol) const
{
  ol.startTextBlock();
  QCString docStr = (briefDescription().isEmpty()?"":briefDescription()+"\n\n")+documentation()+inbodyDocumentation();
  if (hasBriefDescription() && !SectionManager::instance().find(name()))
  {
    ol.pushGeneratorState();
    ol.disableAllBut(OutputType::Man);
    ol.writeString(" - ");
    ol.popGeneratorState();
  }
  ol.generateDoc(
      docFile(),           // fileName
      docLine(),           // startLine
      this,                // context
      0,                   // memberdef
      docStr,              // docStr
      TRUE,                // index words
      FALSE,               // not an example
      QCString(),                   // exampleName
      FALSE,               // singleLine
      FALSE,               // linkFromIndex
      TRUE                 // markdown support
      );
  ol.endTextBlock();

  if (hasSubPages())
  {
    // for printed documentation we write subpages as section's of the
    // parent page.
    ol.pushGeneratorState();
    ol.disableAll();
    ol.enable(OutputType::Latex);
    ol.enable(OutputType::Docbook);
    ol.enable(OutputType::RTF);

    for (const auto &subPage : m_subPages)
    {
      ol.writePageLink(subPage->getOutputFileBase(), FALSE);
    }

    ol.popGeneratorState();
  }
}

bool PageDefImpl::visibleInIndex() const
{
  bool externalPages = Config_getBool(EXTERNAL_PAGES);
  return // not part of a group
         !getGroupDef() &&
         // not an externally defined page
         (!isReference() || externalPages);
}

bool PageDefImpl::documentedPage() const
{
   return // not part of a group
          !getGroupDef() &&
          // not an externally defined page
          !isReference();
}

bool PageDefImpl::hasSubPages() const
{
  return !m_subPages.empty();
}

void PageDefImpl::setNestingLevel(int l)
{
  m_nestingLevel = l;
}

void PageDefImpl::setLocalToc(const LocalToc &lt)
{
  m_localToc = lt;
}

void PageDefImpl::setShowLineNo(bool b)
{
  m_showLineNo = b;
}

bool PageDefImpl::showLineNo() const
{
  return m_showLineNo;
}

bool PageDefImpl::hasTitle() const
{
  return !m_title.isEmpty() && m_title.lower()!="notitle";
}

void PageDefImpl::setTitle(const QCString &title)
{
  m_title = title;
}

// --- Cast functions

PageDef *toPageDef(Definition *d)
{
  if (d==0) return 0;
  if (d && typeid(*d)==typeid(PageDefImpl))
  {
    return static_cast<PageDef*>(d);
  }
  else
  {
    return 0;
  }
}

const PageDef *toPageDef(const Definition *d)
{
  if (d==0) return 0;
  if (d && typeid(*d)==typeid(PageDefImpl))
  {
    return static_cast<const PageDef*>(d);
  }
  else
  {
    return 0;
  }
}

