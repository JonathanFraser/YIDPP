#ifndef YIDPP_PARSER_H_
#define YIDPP_PARSER_H_
#include <vector>
#include <memory>
#include <utility>
#include <set>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <iterator>

namespace yidpp {
		template<class T,class A>
		class Parser;


		//Data object for fixed point computation
		class ChangeCell {
			public:
				ChangeCell() : change(false) {};
				bool change;
				std::set<void*> seen;
				bool orWith(bool change) {
					this->change = this->change || change;
					return this->change;
				}
		};

		struct Node {
			void* item;
			std::string label;
			std::vector<void*> children;
		};

		typedef std::unordered_map<void*,Node> Graph;

	//The abstract base class for all parsers
	template<class T, class A>
	class Parser : public std::enable_shared_from_this<Parser<T,A>> {
		public:
			Parser() : initialized(false) {};

			//retreive the parse forest because the stream has terminated
			std::set<A> parseNull() {
				if(isEmpty()) {
					return std::set<A>();
				} else {
					init();
					return parseNullLocal;
				}
			}

			//getter for Nullable that performs a lazy fixedpoint
			bool isNullable() {
				if(isEmpty()) {
					return false;
				} else {
					init();
					return isNullableLocal;
				}
			}

			//getter for Empty that performs a lazy fixed point
			bool isEmpty() {
				init();
				return isEmptyLocal;
			}

			//take the derivative with respect to a terminal pretty much the main algorithm
			std::shared_ptr<Parser<T,A>> derive (T t) {
				
				//should do an is empty check here 
				if (cache.count(t)) {
					return cache.find(t)->second; //if seen before return previous result
				} else {
						std::shared_ptr<Parser<T,A>> temp_derive = internalDerive(t);  //new get the internal derivative
						cache.insert(std::make_pair(t,temp_derive));
						return temp_derive;
				}
			}

			//parse the entire input stream and return the forest
			virtual std::set<A> parseFull(const std::vector<T>& input) { 
				if (input.empty()) {
					return parseNull();
				} else{  
					std::vector<T> remainder(++input.begin(), input.end());
					return derive(input.front())->parseFull(remainder);
				}
			}
		
			//parse the available input and get the interim state
			virtual std::set<std::pair<A,std::vector<T>>> parse(const std::vector<T>& input) {
				if (input.empty()) { 
					std::set<std::pair<A,std::vector<T>>> parseSet;
					std::set<A> parseNullResult = parseNull();
					for(typename std::set<A>::iterator i=parseNullResult.begin(); i != parseNullResult.end(); ++i) {
						parseSet.insert(std::make_pair(*i,std::vector<T>()));
					}
					return parseSet;
				} else { 
					std::set<std::pair<A,std::vector<T>>> nonEmpty = derive(input.front())->parse(std::vector<T>(++input.begin(),input.end()));
					std::set<A> parseFullResult = parseFull(std::vector<T>());
					for(typename std::set<A>::iterator i=parseFullResult.begin();i!=parseFullResult.end();++i) {
						nonEmpty.insert(std::make_pair(*i,input));
					}
					return nonEmpty;
				}
			}
		
			//virtual method for fixed point computation
			void updateChildBasedAttributes(ChangeCell& change) {
				void* localPtr = this;
				if(!change.seen.count(localPtr)) {
					change.seen.insert(localPtr);
					oneShotUpdate(change);
					initialized=true;
				}
				allUpdate(change);
			}

			virtual void treeRecurse(Graph& valueSet) {
				if(!valueSet.count(this)) {
					Node temp;
					temp.label = this->getLabel();
					temp.children = this->getChildren();
					temp.item = this;
					valueSet.insert(std::make_pair(static_cast<void*>(this),temp));
					this->recurseChildren(valueSet);
				}
			}

			virtual std::vector<void*> getChildren() {
				return std::vector<void*>();
			}

			virtual void recurseChildren(Graph& valueSet) {};

			virtual std::string getLabel() {
				return "UNKNOWN";
			}

		protected:
			//has the fixed point been done
			bool initialized;
			
			//virtual method for popping the derivative
			virtual std::shared_ptr<Parser<T,A>> internalDerive (T t) = 0;

			//setter for parse forest and checks if changed
			bool parseNullSet(std::set<A> set) {
				if(parseNullLocal != set) {
					parseNullLocal = set;
					return true;
				} else {
					return false;
				}
			}

			//setter for local is empty and checks if changed
			bool isEmptySet(bool v) {
				if(isEmptyLocal != v) {
					isEmptyLocal = v;
					return true;
				} else {
					return false;
				}
			}

			//setter for local nullable and checks if changed
			bool isNullableSet(bool v) {
				if(isNullableLocal != v) {
					isNullableLocal = v;
					return true;
				} else {
					return false;
				}
			}


			virtual void oneShotUpdate(ChangeCell&) {};
			virtual void allUpdate(ChangeCell&) {};
			
			//contains the current parse forest for it
			std::set<A> parseNullLocal;
			
			//Nullable and Empty status of the class
			bool isEmptyLocal = false;
			bool isNullableLocal = false;

		private:
			
			//cache of derivative results
			std::unordered_map<T,std::shared_ptr<Parser<T,A>>> cache;
			
			//performs the fixed point update of the properties
			void init() {
				if(initialized)
					return;
				ChangeCell change;
				do {
					change = ChangeCell();
					updateChildBasedAttributes(change);
				} while(change.change);
			}
};


template<class T, class A>
class DFut : public Parser<T,A> {
		std::shared_ptr<Parser<T,A>> to_derive;
		T terminal;

	public:
		DFut(std::shared_ptr<Parser<T,A>> input, T terminal) : to_derive(input) , terminal(terminal) {};
	
		virtual std::set<std::pair<A,std::vector<T>>> parse(const std::vector<T>& input) override {
			return to_derive->derive(terminal)->parse(input);
		}



			virtual std::vector<void*> getChildren() {
				std::vector<void*> temp;
				temp.push_back(to_derive.get());
				return temp;
			}

			virtual void recurseChildren(Graph& valueSet) {
				to_derive->treeRecurse(valueSet);
			};

			virtual std::string getLabel() {
				return "DerivativeFuture";
			}

	protected:

		std::shared_ptr<Parser<T,A>> internalDerive(T t) override {
			return to_derive->derive(terminal)->derive(t);
		}

		virtual void oneShotUpdate(ChangeCell &change) override {
      to_derive->derive(terminal)->updateChildBasedAttributes(change);
		}

		virtual void allUpdate(ChangeCell& change) override {
			change.orWith (Parser<T,A>::parseNullSet(to_derive->derive(terminal)->parseNull()));
			change.orWith (Parser<T,A>::isEmptySet(to_derive->derive(terminal)->isEmpty()));
			change.orWith (Parser<T,A>::isNullableSet(to_derive->derive(terminal)->isNullable()));
		}

};


template<class T,class A>
	class RecursiveParser : public Parser<T,A> {
			std::shared_ptr<Parser<T,A>> recursive;

	public:
		RecursiveParser(std::shared_ptr<Parser<T,A>> input) : recursive(input) {};
		RecursiveParser() {};

		void SetRecurse(std::shared_ptr<Parser<T,A>> input) { recursive = input; }
	
		virtual std::set<std::pair<A,std::vector<T>>> parse(const std::vector<T>& input) override {
			return recursive->parse(input);
		}
			
			virtual std::vector<void*> getChildren() {
				std::vector<void*> temp;
				temp.push_back(recursive.get());
				return temp;
			}

			virtual void recurseChildren(Graph& valueSet) {
				recursive->treeRecurse(valueSet);
			};

			virtual std::string getLabel() {
				return "RecursiveParser";
			}

	protected:

		std::shared_ptr<Parser<T,A>> internalDerive(T t) override {
			return std::make_shared<DFut<T,A>>(recursive,t);
		}

		virtual void oneShotUpdate(ChangeCell &change) override {
      recursive->updateChildBasedAttributes(change);
		}

		virtual void allUpdate(ChangeCell& change) override {
			change.orWith (Parser<T,A>::parseNullSet(recursive->parseNull()));
			change.orWith (Parser<T,A>::isEmptySet(recursive->isEmpty()));
			change.orWith (Parser<T,A>::isNullableSet(recursive->isNullable()));
		}
	};

//class for the empty set
template<class T, class A>
class Emp : public Parser<T,A> {
	public:
		Emp() {
			//always empty
			Parser<T,A>::isEmptySet(true);
			//never nullable
			Parser<T,A>::isNullableSet(false);
		}
		
		//you get nothing out of parsing it
		virtual std::set<std::pair<A,std::vector<T>>> parse(const std::vector<T>& input) override {
			return std::set<std::pair<A,std::vector<T>>>();
		} //you get nothing out of parsing it
  
		std::string getLabel() override {
			return "Empty_Set";
		}
  protected:
		
		std::shared_ptr<Parser<T,A>> internalDerive (T t) override {
			return Parser<T,A>::shared_from_this();
		}//derivative of the empty set is the empty set
};


//class for the empty string or the Nulll reduction parser
template<class T, class A>
class Eps : public Parser<T,A> {
	public:
		Eps(std::set<A> generator) {
			//The empty string is not the Null Set
			//it contains only the empty string and is
			//thus non empty
			Parser<T,A>::isEmptySet(false);
			
			//It by definition contains the empty string
			//and is therefore nullable
			Parser<T,A>::isNullableSet(true);
			
			//The Character consumed (or possible parse trees consumed) to produce this is
			//what should be returned on a null parse
			Parser<T,A>::parseNullSet(generator);
		}


		std::string getLabel() override {
			return "Empty_String";
		}
	protected:

		//if you take the derivative of it you get the null set
		//which is no parser at all
		virtual std::shared_ptr<Parser<T,A>> internalDerive(T t) override {
			return std::make_shared<Emp<T,A>>();
		}

	public:
	virtual	std::set<std::pair<A,std::vector<T>>> parse(const std::vector<T>& input) override {
			std::set<std::pair<A,std::vector<T>>> retval;
			auto parseNullResult = Parser<T,A>::parseNull();
			for(auto i = parseNullResult.begin(); i!= parseNullResult.end(); ++i) {
				retval.insert(std::make_pair(*i,input));
			}
			return retval;
		}
};


//parser for a single terminal
template<class T>
class EqT : public Parser<T,T> {
	private:
		T t;
	public:
		EqT(T t) : t(t) {
			//A single character (terminal) is neither empty nor nullable as it contains
			//and item (the single terminal) and does not contain the empty string
			Parser<T,T>::isEmptySet(false);
			Parser<T,T>::isNullableSet(false);
		}
	
		virtual std::set<std::pair<T,std::vector<T>>> parse(const std::vector<T>& input) override {
				std::set<std::pair<T,std::vector<T>>> retval;
				if(!input.empty() && input.front() == t) {
					retval.insert(std::make_pair(input.front(), std::vector<T>(++input.begin(), input.end())));
				}
				return retval;
			}
  
		std::string getLabel() override {
			return "TerminalParser";
		}
  protected:

		virtual std::shared_ptr<Parser<T,T>> internalDerive(T t_) override {
			if(t == t_) {
				//derivative of the single terminal is the null reduction parser
				//formed with t as the construction option
				std::set<T> generator;
				generator.insert(t);
				return std::make_shared<Eps<T,T>>(generator);
			} else {
				//if not equal cannot be part of language
				//therefore null set or empty parser
				return std::make_shared<Emp<T,T>>();
			}
		}
};

//Union
template<class T,class A>
class Alt : public Parser<T,A> {
	private:
		std::set<std::shared_ptr<Parser<T,A>>> unioned_parsers;

	public:
		Alt(std::set<std::shared_ptr<Parser<T,A>>> parsers) : unioned_parsers(parsers) { }

		virtual std::vector<void*> getChildren() {
			std::vector<void*> temp;
			for(auto i=unioned_parsers.begin();i!=unioned_parsers.end(); ++i) {
				temp.push_back(i->get());
			}
			return temp;
		}

		virtual void recurseChildren(Graph& valueSet) {
			for(auto i=unioned_parsers.begin(); i!=unioned_parsers.end(); ++i) {
				(*i)->treeRecurse(valueSet);
			}
		};

		std::string getLabel() override {
			return "Union";
		}
	protected:

		virtual std::shared_ptr<Parser<T,A>> internalDerive(T t) override {
			//Quick optimization
			//if a choice is empty dont include it in the next union
			std::set<std::shared_ptr<Parser<T,A>>> NewSet;
			for(auto i=unioned_parsers.begin();i!=unioned_parsers.end();++i) {
				if(!((*i)->isEmpty())) {
						NewSet.insert((*i)->derive(t));
				}
			}

			//If it is a singlton set then don't bother with the union wrapper
			//just return the parser
			if(NewSet.size() == 1) {
				return *NewSet.begin();
			}
			
			return std::make_shared<Alt<T,A>>(NewSet);
		}

	protected:
		virtual void oneShotUpdate(ChangeCell &change) override {
			for(auto i=unioned_parsers.begin();i!=unioned_parsers.end();++i) {
				(*i)->updateChildBasedAttributes(change);
			}
		}

		virtual void allUpdate(ChangeCell& change) override {
			std::set<A> nullSet;
			bool tempEmpty = true;
			bool tempNullable = false;
			for(auto i=unioned_parsers.begin();i!=unioned_parsers.end();++i) {
				auto temp = (*i)->parseNull();
				nullSet.insert(temp.begin(),temp.end());
				tempEmpty = tempEmpty && (*i)->isEmpty();
				tempNullable = tempNullable || (*i)->isNullable(); 
			}
			
			change.orWith (Parser<T,A>::parseNullSet(nullSet));
			change.orWith (Parser<T,A>::isEmptySet(tempEmpty));
			change.orWith (Parser<T,A>::isNullableSet(!this->isEmpty() && tempNullable));
		}
		
};



template<class T, class A, class B>
class Con : public Parser<T,std::pair<A,B>> {
 	private:
		std::shared_ptr<Parser<T,A>> first;
		std::shared_ptr<Parser<T,B>> second;
	public:
		Con(std::shared_ptr<Parser<T,A>> first, std::shared_ptr<Parser<T,B>> second) : first(first), second(second) {};

		virtual std::vector<void*> getChildren() {
			std::vector<void*> temp;
			temp.push_back(first.get());
			temp.push_back(second.get());
			return temp;
		}

		virtual void recurseChildren(Graph& valueSet) {
			first->treeRecurse(valueSet);
			second->treeRecurse(valueSet);
		};

		std::string getLabel() override {
			return "Concatenation";
		}
	protected:
		virtual std::shared_ptr<Parser<T,std::pair<A,B>>> internalDerive(T t) override {

			//Concatenation has some funny rules and is also a big target of compaction
			//so apply it here
			std::shared_ptr<Parser<T,A>> leftDerive = first->derive(t);
			std::shared_ptr<Parser<T,std::pair<A,B>>> primaryRet = std::make_shared<Emp<T,std::pair<A,B>>>();
			if(!leftDerive->isEmpty()) { 
				//only if the first expressions derivative is non-empty do we care about the first
				//concatenation in the union
				primaryRet = std::make_shared<Con<T,A,B>>(leftDerive, second);
			}

			if(first->isNullable()) {
				//If the first language is nullable, then the second term in the rule appears
				//and we need to generate the null reduction (empty string parser) for the first
				//term
				std::shared_ptr<Parser<T,A>> nullability = std::make_shared<Eps<T,A>>(first->parseNull());
				//we also want the second derivate to concat with the reduction parser
				std::shared_ptr<Parser<T,B>> rightDerive = second->derive(t);
				if(leftDerive->isEmpty()) {
					//if the left is empty then only worry about nullability portion of the concat
					if(rightDerive->isEmpty()) {
						//if both right and left are empty, then return the Null parser because its screwed anyway
						return primaryRet;
					} else {
						//just the right side of the alternation rule 
						return std::make_shared<Con<T,A,B>>(nullability,rightDerive);
					}
				} else {
					//There is nothing we can do to optimize, so just take the full rule
					std::set<std::shared_ptr<Parser<T,std::pair<A,B>>>> retSet;
					retSet.insert(primaryRet);
					retSet.insert(std::make_shared<Con<T,A,B>>(nullability,rightDerive));
					return std::make_shared<Alt<T,std::pair<A,B>>>(retSet);
				}
			} else {
				//The second side is not nullable (there for the nullability operation nulls out the right half)
				//so just take the first term of the rule
				return primaryRet;
			}
		}

	protected:
		virtual void oneShotUpdate(ChangeCell& change) override {
				first->updateChildBasedAttributes(change);
				second->updateChildBasedAttributes(change);
		}

		virtual void allUpdate(ChangeCell& change) override {
			std::set<std::pair<A,B>> options;
			auto firstNull = first->parseNull();
			auto secondNull = second->parseNull();
			for(auto i=firstNull.begin();i!=firstNull.end(); ++i) {
				for(auto j=secondNull.begin();j!=secondNull.end(); ++j) {
					options.insert(std::make_pair(*i,*j));
				}
			}
			change.orWith(Parser<T,std::pair<A,B>>::parseNullSet(options));
			change.orWith(Parser<T,std::pair<A,B>>::isEmptySet(first->isEmpty() || second->isEmpty()));
			change.orWith(Parser<T,std::pair<A,B>>::isNullableSet(!Parser<T,std::pair<A,B>>::isEmpty() &&(first->isNullable() && second->isNullable())));
		}
};


//Reducton Operator
template<class T, class A, class B>
class Red : public Parser<T,B> {
  private:
		std::shared_ptr<Parser<T,A>> localParser;
		std::function<B(A)> reductionFunction;
	public:
		Red(std::shared_ptr<Parser<T,A>> parser, std::function<B(A)> redfunc): localParser(parser), reductionFunction(redfunc) {};	
		
		virtual std::vector<void*> getChildren() {
			std::vector<void*> temp;
			temp.push_back(localParser.get());
			return temp;
		}

		virtual void recurseChildren(Graph& valueSet) {
			localParser->treeRecurse(valueSet);
		};

		std::string getLabel() override {
			return "ReductionOperation";
		}
	
	protected:
		virtual std::shared_ptr<Parser<T,B>> internalDerive(T t) override {
			
			auto temp = localParser->derive(t);
			//If the derivitive of the internal parser which you are reducing is the Null Parser
			//Then the result is simply the Null Parser of the correct type
			if(temp->isEmpty()) {
				return std::make_shared<Emp<T,B>>();
			}
			
			//derivative of the reduction is the reduction of the derivative
			return std::make_shared<Red<T,A,B>>(temp,reductionFunction);
		}

	public:
		virtual std::set<B> parseFull(const std::vector<T> &input) override {
			auto lower = localParser->parseFull(input);
			std::set<B> retval;
			//Just take the internal Parse Forest and reduce it.
			for(auto i=lower.begin();i!=lower.end();++i) {
				retval.insert(reductionFunction(*i));
			}
			return retval;
		}

		virtual std::set<std::pair<B,std::vector<T>>> parse(const std::vector<T>& input) override {
			std::set<std::pair<A,std::vector<T>>> lower = localParser->parse(input);
			std::set<std::pair<B,std::vector<T>>> retval;
			for(auto i=lower.begin();i!=lower.end();++i) {
				retval.insert(std::make_pair(reductionFunction(i->first),i->second));
			}
			return retval;
		}

	protected:
		virtual void oneShotUpdate(ChangeCell& change) override {
				localParser->updateChildBasedAttributes(change);
		}

		virtual void allUpdate(ChangeCell& change) override {
			auto localParseNull = localParser->parseNull();
			std::set<B> changedParseNull;
			for(auto i=localParseNull.begin();i!=localParseNull.end();++i) {
				changedParseNull.insert(reductionFunction(*i));
			}
			change.orWith(Parser<T,B>::parseNullSet(changedParseNull));
			change.orWith(Parser<T,B>::isEmptySet(localParser->isEmpty()));
			change.orWith(Parser<T,B>::isNullableSet(localParser->isNullable()));
		}
};

//Kleene Star
template<class T, class A>
class Rep: public Parser<T,std::vector<A>> {
 	private:
		std::shared_ptr<Parser<T,A>> internal;
	public:
		Rep(std::shared_ptr<Parser<T,A>> p) : internal(p) {
			Parser<T,std::vector<A>>::isEmptySet(false);
			Parser<T,std::vector<A>>::isNullableSet(true);
			std::set<std::vector<A>> retset;
			retset.insert(std::vector<A>());
			Parser<T,std::vector<A>>::parseNullSet(retset);
		}
		
		virtual std::vector<void*> getChildren() {
			std::vector<void*> temp;
			temp.push_back(internal);
			return temp;
		}

		virtual void recurseChildren(Graph& valueSet) {
			internal->treeRecurse(valueSet);
		};

		std::string getLabel() override {
			return "Kleene";
		}
	
	protected:
	
	static std::vector<A> reductionOperation(std::pair<A,std::vector<A>> input) {
					std::vector<A> retval; 
					retval.push_back(input.first);
					retval.insert(retval.begin(),input.second.begin(),input.second.end());
					return retval;
	}
	
	virtual std::shared_ptr<Parser<T,std::vector<A>>> internalDerive(T t) override {
			auto localDerive = std::make_shared<Con<T,A,std::vector<A>>>(internal->derive(t), Parser<T,std::vector<A>>::shared_from_this());
			return std::make_shared<Red<T,std::pair<A,std::vector<A>>,std::vector<A>>>(localDerive,Rep<T,A>::reductionOperation);
		}

	protected:
		virtual void oneShotUpdate(ChangeCell& change) override {
				internal->updateChildBasedAttributes(change);
				Parser<T,std::vector<A>>::initialized=true;
		}
};

std::string ptr2string(void* pointer) {
	std::stringstream sstream;
	sstream << "Pointer" << pointer;
	return sstream.str();
}

std::string printSingleRelation(void* parent, void* child) {
	std::string retval;
	retval += ptr2string(parent);
	retval += "->";
	retval += ptr2string(child);
	retval += ";\n";
	return retval;
}

std::string printNodeRelations(const Node& node) {
	std::string retval;
	for(auto i=node.children.cbegin();i!=node.children.cend();++i) {
		retval += printSingleRelation(node.item,*i);
	}
	return retval;
}

std::string printGraphRelations(const Graph& graph) {
	std::string retval;
	for(auto i=graph.cbegin();i!=graph.cend();++i) {
		retval += printNodeRelations(i->second);
	}
	return retval;
}

std::string formatNodeLabel(const Node& node) {
	std::string retvalue;
	retvalue += ptr2string(node.item);
	retvalue += " [label="+node.label+"];\n";
	return retvalue;
}

std::string printNodeLabels(const Graph& graph) {
	std::string nodeLabels;
	for(auto i=graph.cbegin();i!=graph.cend();++i) {
		nodeLabels+= formatNodeLabel(i->second);
	}
	return nodeLabels;
}

std::string printGraph(const std::string &name, const Graph& graph) {
	std::string retval;
	retval = "digraph ";
	retval += name;
	retval += " {\n";
	retval += printNodeLabels(graph);
	retval += printGraphRelations(graph);
	retval += "}\n";
	return retval;
}


template<class T, class A>
std::string getGraph(const std::string &name,std::shared_ptr<Parser<T,A>> input_parser) {
	Graph info;
	input_parser->treeRecurse(info);
	return printGraph(name,info);
}

};


#endif
