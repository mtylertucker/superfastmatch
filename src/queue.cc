#include "queue.h"

namespace superfastmatch{
  
  Queue::Queue(Registry* registry):
  registry_(registry){}

  uint64_t Queue::add_document(const uint32_t doc_type,const uint32_t doc_id,const string& content,bool associate){
    return CommandFactory::addDocument(registry_,doc_type,doc_id,content,associate);
  }

  uint64_t Queue::delete_document(const uint32_t& doc_type,const uint32_t& doc_id){
    return CommandFactory::dropDocument(registry_,doc_type,doc_id);
  }
  
  uint64_t Queue::addAssociations(const uint32_t& doc_type){
    return CommandFactory::addAssociations(registry_,doc_type);
  }
  
  bool Queue::process(){
    Logger* logger=registry_->getLogger();
    stringstream message;
    bool workDone=false;
    CommandType batchType=Invalid;
    deque<Command*> batch;
    vector<Command*> work;
    while(CommandFactory::getNextBatch(registry_,batch,batchType)){
      workDone=true;
      switch (batchType){
        case AddDocument:
          while(!batch.empty()){
            Document* doc = batch.front()->getDocument();
            //Check if document exists and insert drop if it does
            if (doc->save()){
              message << "Saved: " << *doc;
              work.push_back(batch.front());
              batch.pop_front();  
            }else{
              message << "Inserting drop for: " << *doc;
              CommandFactory::insertDropDocument(registry_,batch.front());
              break;
            }
            logger->log(Logger::DEBUG,&message);
            delete doc;
          }
          registry_->getPostings()->addDocuments(work);
          for(vector<Command*>::iterator it=work.begin(),ite=work.end();it!=ite;++it){
            (*it)->setFinished();
          }
          break;  
        case DropDocument:
          message << "Dropping Document(s)";
          logger->log(Logger::DEBUG,&message);
          while(!batch.empty()){
            work.push_back(batch.front());
            batch.pop_front();  
          }
          registry_->getPostings()->deleteDocuments(work);
          for (vector<Command*>::iterator it=work.begin(),ite=work.end();it!=ite;++it){
            Document* doc = (*it)->getDocument();
            if (not(doc->remove())){
              (*it)->setFailed();
            }else{
              (*it)->setFinished();
            }
            delete doc;
          }
          break;
        case AddAssociation:
          message << "Adding Association(s)";
          logger->log(Logger::DEBUG,&message);
          while(!batch.empty()){;
            work.push_back(batch.front());
            batch.pop_front();
          }
          registry_->getPostings()->addAssociations(work);
          for(vector<Command*>::iterator it=work.begin(),ite=work.end();it!=ite;++it){
            (*it)->setFinished();
          }
          break;
        case AddAssociations:{
            DocumentCursor* cursor = new DocumentCursor(registry_);
            Document* doc;
            while((doc=cursor->getNext())!=NULL){
              CommandFactory::insertAddAssociation(registry_,doc->doctype(),doc->docid(),batch.front());
              delete doc;
            }
            delete cursor;
            batch.front()->setFinished();
          }
          break;
        case DropAssociation:
          message << "Dropping Association(s)";
          logger->log(Logger::DEBUG,&message);
          while(!batch.empty()){
            work.push_back(batch.front());
            batch.pop_front();
          }
          //Do Drop Association
          for(vector<Command*>::iterator it=work.begin(),ite=work.end();it!=ite;++it){
            (*it)->setFinished();
          }
          break;
        case Invalid:
          throw("Bad batch type");
          break;
      }
      FreeClear(batch);
      FreeClear(work);
    }
    return workDone;
  }
  
  bool Queue::purge(){
    return true;
  }
  
  void Queue::fill_list_dictionary(TemplateDictionary* dict){
    vector<Command*> commands;
    CommandFactory::getAllCommands(registry_,commands);
    for (vector<Command*>::iterator it=commands.begin(),ite=commands.end();it!=ite;++it){
      TemplateDictionary* command_dict=dict->AddSectionDictionary("COMMAND");
      (*it)->fill_list_dictionary(command_dict);
    }
    FreeClear(commands);
  }
  
  void Queue::fill_item_dictionary(TemplateDictionary* dict,const uint64_t queue_id){
    
  }
}