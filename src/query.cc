#include <query.h>
#include <document.h>

namespace superfastmatch
{
  DocTypeRange::DocTypeRange(){}
  
  bool DocTypeRange::parse(const string& range){
    if (range.empty()){
      return true;
    }
    bool valid=true;
    istringstream iss(range);
    string section;
    while(!iss.eof()){
      getline(iss,section,';');
      size_t splits = count(section.begin(),section.end(),'-');
      if (splits>1){
        valid=false;
        continue;
      }else if (splits==1){
        size_t split=section.find("-");
        uint32_t start=kc::atoi(section.substr(0,split).c_str());
        uint32_t end=kc::atoi(section.substr(split+1).c_str());
        start = min(start,end);
        end = max(start,end);
        if (start!=0 && end!=0){
          for (uint32_t i=start;i<=end;i++){
            doctypes_.insert(i);
          } 
        }else{
          valid=false;
        }
      }else{
        uint32_t single=kc::atoi(section.c_str());
        if (single!=0){
          doctypes_.insert(single);
        }else{
          valid=false;
        }
      }
    }
    if (!valid){
      doctypes_.clear();
    }
    return valid;
  }
  
  bool DocTypeRange::isInRange(uint32_t doctype) const{
    return (doctypes_.size()==0 || doctypes_.find(doctype)!=doctypes_.end());
  }
  
  set<uint32_t> DocTypeRange::getDocTypes(){
    return doctypes_;
  }
  
  DocumentQuery::DocumentQuery(Registry* registry, const string& command):
  registry_(registry),
  order_by_("doctype"),
  desc_(false),
  valid_(true)
  {
    map<string,string> elements,parameters;
    vector<string> paths,sections,parameter;
    urlbreak(command.c_str(),&elements);
    path_=elements["path"];
    kc::strsplit(path_, '/', &paths);
    if (paths.size()>3){
      valid_&=target_.parse(paths[3]);
    }
    if(paths.size()>2){
      valid_&=source_.parse(paths[2]);
    }
    kc::strsplit(elements["query"],'&',&sections);
    for(vector<string>::iterator it=sections.begin(),ite=sections.end();it!=ite;++it){
      kc::strsplit(*it,'=',&parameter);
      if(parameter.size()==2){
        size_t ksiz,vsiz;
        char* kbuf = kc::urldecode(parameter[0].c_str(), &ksiz);
        char* vbuf = kc::urldecode(parameter[1].c_str(), &vsiz);
        parameters[kbuf]=vbuf;
        delete[] kbuf;
        delete[] vbuf;
      }
    }
    cursor_=parameters["cursor"];
    if (parameters.find("order_by")!=parameters.end()){
      if (parameters["order_by"][0]=='-'){
        order_by_=parameters["order_by"].substr(1);
        desc_=true;
      }else{
        order_by_=parameters["order_by"];
        desc_=false;
      }
    }
    limit_=kc::atoi(parameters["limit"].c_str());
    if (limit_==0){
      limit_=registry->getPageSize();
    }
  }

  const bool DocumentQuery::isValid(){
    return valid_;
  }
  
  const string& DocumentQuery::getCursor() const{
    return cursor_;
  }
  
  const string& DocumentQuery::getOrder() const{
    return order_by_;
  }
  
  const bool DocumentQuery::isDescending() const{
    return desc_;
  }
  
  const uint64_t DocumentQuery::getLimit() const{
    return limit_;
  }

  const vector<DocPair>& DocumentQuery::getSourceDocPairs(bool unlimited){
    if (source_pairs_.size()==0){
      source_pairs_=getDocPairs(source_,order_by_,cursor_,unlimited?numeric_limits<uint64_t>::max():limit_,desc_);
    }
    return source_pairs_;
  }
  
  const vector<DocPair>& DocumentQuery::getTargetDocPairs(bool unlimited){
    if (target_pairs_.size()==0){
      target_pairs_=getDocPairs(target_,order_by_,cursor_,unlimited?numeric_limits<uint64_t>::max():limit_,desc_);
    }
    return target_pairs_;
  }
  
  const string& DocumentQuery::getFirst(){
    if (first_.empty()){
      first_=getCommand();
    }
    return first_;
  }
  
  const string& DocumentQuery::getLast(){
    if (last_.empty()){
      vector<DocPair> pairs=getDocPairs(source_,order_by_,"",limit_,!desc_);
      if (pairs.size()>0){
        last_=getCommand(getDocPairs(source_,order_by_,"",limit_,!desc_).back()); 
      }
    }
    return last_;
  }
  
  const string& DocumentQuery::getNext(){
    if (next_.empty() && getSourceDocPairs().size()>0){
      string cursor=getCursor(getSourceDocPairs().back());
      vector<DocPair> pairs=getDocPairs(source_,order_by_,cursor,2,desc_);
      if (pairs.size()>0){
        next_=getCommand(pairs.back());
      }else{
        next_=getLast();
      }
    }
    return next_;
  }
  
  const string& DocumentQuery::getPrevious(){
    if (previous_.empty() && getSourceDocPairs().size()>0){
      string cursor=getCursor(getSourceDocPairs().front());
      vector<DocPair> pairs=getDocPairs(source_,order_by_,cursor,limit_+1,!desc_);
      if (pairs.size()>0){
        previous_=getCommand(pairs.back());
      }else{
        previous_=getFirst();
      }
    }
    return previous_;
  }
  
  const string DocumentQuery::getCursor(const DocPair& pair)const{
    DocumentPtr doc=registry_->getDocumentManager()->getDocument(pair.doc_type,pair.doc_id,DocumentManager::META);
    string cursor;
    if (doc){
      cursor=doc->getMeta(order_by_);
      cursor=kc::urlencode(cursor.c_str(), cursor.size());
      cursor+=":"+toString(doc->doctype())+":"+toString(doc->docid());
    }
    return cursor;
  }
  
  const string DocumentQuery::parseCursor(const string& cursor)const{
    size_t splits = count(cursor.begin(),cursor.end(),':');
    if (splits!=2){
      return cursor;
    }
    size_t csiz;
    size_t first=cursor.find(":");
    size_t second=cursor.find(":",first+1);
    string parsed=cursor.substr(0,first);
    parsed = kc::urldecode(parsed.c_str(),&csiz);
    parsed=padIfNumber(parsed);
    uint32_t doctype=kc::atoi(cursor.substr(first+1,second).c_str());
    if (doctype!=0){
      char key[4];
      doctype=kc::hton32(doctype);
      memcpy(key,&doctype,4);
      parsed+=string(key,4);
    }
    uint32_t docid=kc::atoi(cursor.substr(second+1).c_str());
    if (docid!=0){
      char key[4];
      docid=kc::hton32(docid);
      memcpy(key,&docid,4);
      parsed+=string(key,4);
    }
    return parsed;
  }
  
  const string DocumentQuery::getCommand(const DocPair& pair) const{
    return getCommand()+"&cursor="+getCursor(pair);
  }
  
  const string DocumentQuery::getCommand() const{
    string command(path_+"?limit="+toString(limit_));
    if (!order_by_.empty()){
      command+="&order_by=";
      if(desc_){
        command+="-"; 
      }
      command+=order_by_;
    }
    return command;
  }
  
  vector<DocPair> DocumentQuery::getDocPairs(const DocTypeRange& range, const string& order_by,const string& cursor, const uint64_t limit, const bool desc) const{
    kc::PolyDB::Cursor* cur=registry_->getOrderedMetaDB()->cursor();
    vector<DocPair> pairs;
    string key;
    uint64_t count=0;
    string start=(cursor.size()==0)?order_by:(order_by+parseCursor(cursor));
    if (desc){
      for (size_t i=start.size();i>0;i--){
        if (start[i-1]!=255){
          start.replace(i-1,1,1,start[i-1]+1);
          break;
        }
      }
      if (not cur->jump_back(start)){
        cur->jump_back();
      }
    }else{
      if (not cur->jump(start)){
        cur->jump();
      }
    }
    while((cur->get_key(&key))&&(key.compare(0,order_by.size(),order_by)==0)&&(count<limit)){
      DocPair pair(key.substr(key.size()-8));
      if (range.isInRange(pair.doc_type)){
        pairs.push_back(pair);
        count++;
      }
      if (desc){
        if (not cur->step_back()){
          break;
        }
      }else if(not cur->step()){
        break;
      }
    }
    delete cur;
    return pairs;
  } 
  
  void DocumentQuery::fillListDictionary(TemplateDictionary* dict){
    dict->SetValueAndShowSection("PAGE",getFirst(),"FIRST");
    dict->SetValueAndShowSection("PAGE",getLast(),"LAST");
    dict->SetValueAndShowSection("PAGE",getPrevious(),"PREVIOUS");
    dict->SetValueAndShowSection("PAGE",getNext(),"NEXT");
    set<string> keys_set;
    vector<DocumentPtr> docs;
    vector<string> keys;
    for(vector<DocPair>::const_iterator it=getSourceDocPairs().begin(),ite=getSourceDocPairs().end();it!=ite;++it){
      DocumentPtr doc=registry_->getDocumentManager()->getDocument(it->doc_type,it->doc_id,DocumentManager::META);
      docs.push_back(doc);
      if (doc->getMetaKeys(keys)){
        for (vector<string>::iterator it=keys.begin();it!=keys.end();it++){
          keys_set.insert(*it);
        }
        keys_set.erase("docid");
        keys_set.erase("doctype");
      }
    }
    for (set<string>::const_iterator it=keys_set.begin(),ite=keys_set.end();it!=ite;++it){
      TemplateDictionary* keys_dict=dict->AddSectionDictionary("KEYS");
      keys_dict->SetValue("KEY",*it);
      if ((!desc_) && (*it==order_by_)){
        keys_dict->SetValue("DIRECTION","-");
      }
    }
    for (vector<DocumentPtr>::iterator it=docs.begin(),ite=docs.end();it!=ite;++it){
      TemplateDictionary* doc_dict = dict->AddSectionDictionary("DOCUMENT");
      doc_dict->SetIntValue("DOC_TYPE",(*it)->doctype());
      doc_dict->SetIntValue("DOC_ID",(*it)->docid()); 
      for (set<string>::const_iterator it2=keys_set.begin(),ite2=keys_set.end();it2!=ite2;++it2){
        TemplateDictionary* values_dict=doc_dict->AddSectionDictionary("VALUES");
        values_dict->SetValue("VALUE",(*it)->getMeta(&(*it2->c_str())));
      }
    }
  }
}
