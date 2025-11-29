#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;


namespace AnalysisAlgorithm 
{
        template<typename T>
        bool isClassObject(T* varDecl, CXXRecordDecl* clssDecl)
        {
            if (!(varDecl && clssDecl)) return false;
            return !varDecl->getType()->isPointerType() && !varDecl->getType()->isReferenceType()
                && (varDecl->getType()->getCanonicalTypeUnqualified() == clssDecl->getTypeForDecl()->getCanonicalTypeUnqualified());
        }

        void findAssignmentsInStmt(Stmt* stmt, std::vector<BinaryOperator*>& assignments) 
        {
            if (!stmt) return;
            
            if (auto* binOp = dyn_cast<BinaryOperator>(stmt)) {
                if (binOp->getOpcode() == BO_Assign) {
                    assignments.push_back(binOp);
                }
            }
            
            for (Stmt* child : stmt->children()) {
                findAssignmentsInStmt(child, assignments);
            }
        }


        template<typename T>
        struct function_traits;

        template<typename Ret, typename Class, typename Arg>
        struct function_traits<Ret(Class::*)(Arg) const> {
            using arg_type = Arg;
        };

        template<typename Callable>
        struct function_traits : function_traits<decltype(&Callable::operator())> {};

        template<typename InputIt, typename UnaryPred>
        typename std::iterator_traits<InputIt>::difference_type
        count_if(InputIt first, InputIt last, UnaryPred p)
        {
            using Cast = std::remove_pointer_t<typename function_traits<UnaryPred>::arg_type>;
            typename std::iterator_traits<InputIt>::difference_type ret = 0;
            for (; first != last; ++first) {
                if (auto *c = dyn_cast<Cast>(*first))
                    if (p(c))
                        ++ret;
            }
            return ret;
        }

        int countClassStaticObject(CXXRecordDecl* clssDecl, FunctionDecl* funcDecl)
        {
            if (!funcDecl->hasBody() || !(clssDecl && funcDecl)) return 0;
            
            int count = 0;
            for (Stmt* st : funcDecl->getBody()->children()) {
                if (auto *declStmt = dyn_cast<DeclStmt>(st)) {
                    count += count_if(declStmt->decl_begin(), declStmt->decl_end(), 
                        [&](VarDecl* var) {
                            return isClassObject(var, clssDecl) && var->isStaticLocal();
                        }
                    );
                }
            }
            return count;
        }

        int countClassStaticObject(CXXRecordDecl* clssDecl, CXXRecordDecl* targetClssDecl)
        {
            if (!(clssDecl && targetClssDecl)) return 0;
            
            int count = 0;
            for (auto* field : targetClssDecl->decls()) 
                if (isClassObject(dyn_cast<VarDecl>(field), clssDecl))
                   ++count;
            
            for (auto* method : targetClssDecl->methods()) 
                count += countClassStaticObject(clssDecl, method);
            
            return count; 
        }

        bool findClassLocalObject(CXXRecordDecl* clssDecl, CXXRecordDecl* targetClssDecl)
        {
            if (!(clssDecl && targetClssDecl)) return 0;
           
            for (auto* field : targetClssDecl->fields()) {
                if (field->getType()->isPointerType() || field->getType()->isReferenceType())
                   continue;
                else if (isClassObject(field, clssDecl))
                    return true;
            }
            return false;
        }

        bool findClassLocalObject(CXXRecordDecl* clssDecl, FunctionDecl* funcDecl)
        {
            if (!funcDecl->hasBody() || !(clssDecl && funcDecl)) return false;
            
            for (Stmt* st : funcDecl->getBody()->children()) {
                if (auto *declStmt = dyn_cast<DeclStmt>(st)) {
                    for (Decl* dcl : declStmt->decls()) {
                        if (VarDecl* varDecl = dyn_cast<VarDecl>(dcl)){
                            if (isClassObject(varDecl, clssDecl) && !varDecl->isStaticLocal()) {
                               return true;
                            }
                        }
                    }
                }
            }
            return false;
        }

        bool isSameType(QualType type1, QualType type2) {
          return type1.getTypePtr()->getUnqualifiedDesugaredType() == 
                 type2.getTypePtr()->getUnqualifiedDesugaredType();
        }

        bool compareReturnTypeWithRecordType(FunctionDecl *method, CXXRecordDecl *record) {
            if (!method || !record) return false;
            
            QualType returnType = method->getReturnType();
            QualType recordType = record->getASTContext().getRecordType(record);
            
            if (returnType->isPointerType() || returnType->isReferenceType()) {
                returnType = returnType->getPointeeType();
            }

            return isSameType(returnType.getUnqualifiedType(), 
                             recordType.getUnqualifiedType());
        }

        template<typename Op1, typename Op2>
        bool isEQNEBetween(BinaryOperator* bo) 
        {
           if (!bo) return false; 
           if (bo->getOpcode() != BO_EQ && bo->getOpcode() != BO_NE)
               return false;
           return isa<Op1>(bo->getLHS()->IgnoreImpCasts()) && isa<Op2>(bo->getRHS()->IgnoreImpCasts());
        }

        VarDecl* getVarDeclFromExpr(Expr* expr) {
            if (auto *declRef = dyn_cast<DeclRefExpr>(expr->IgnoreParenCasts())) 
                return dyn_cast<VarDecl>(declRef->getDecl());
            return nullptr;
        }

        VarDecl* extractVarFromUnary(Expr* expr) {
            if (auto* unop = dyn_cast<UnaryOperator>(expr->IgnoreImpCasts())) {
                if (unop->getOpcode() == UO_AddrOf || unop->getOpcode() == UO_Deref) {
                    return getVarDeclFromExpr(unop->getSubExpr()->IgnoreParenCasts());
                }
            }
            return getVarDeclFromExpr(expr->IgnoreImpCasts());
        };
};


namespace {

struct AnalysisData {
    bool ctorsPrivate                   : 1; 
    bool hasMethodLikelyInstance        : 1; 
    bool hasFriendFunctionLikelyInstance: 1; 
    bool hasDeletedCopyConstuctor       : 1; 
    bool hasDeletedAssigmentOperator    : 1;
    bool isSingltone                   : 1;
    bool hiddenInstanceMethod           : 1;
    bool probabalyNaiveSingletone       : 1;
    bool probabalyCRTPSingletone        : 1;
    bool unknownPatternSingletone       : 1;
    bool probablyMayersSingletone       : 1;
    bool probablyFlagsNaiveSingletone   : 1;
    bool probablyIfNaiveSingletone      : 1;
    unsigned int amountObjects          : 27;
    
    enum ConditionPatternInGetInstance {
        UnaryOperatorInCondition,
        BinaryOperatorInConditionNullptr,
        BinaryOperatorInConditionNull,
        VarInCondition,
        UnknownCondition,
    } conditionPatternInGetInstance;
    
    SourceManager* SM = nullptr;
    CXXMethodDecl* methodLikeGetInstance         = nullptr;      
    FunctionDecl* friendFunctionLikeGetInstance  = nullptr;      
    VarDecl* instanceField                       = nullptr;
    BinaryOperator* assignmentInIfSinglton       = nullptr;
    std::string className;
    std::string location;
    
    inline void  clear() noexcept
    {
        probabalyCRTPSingletone = false;
        probablyIfNaiveSingletone = false;
        probablyMayersSingletone = false;
        probablyFlagsNaiveSingletone = false;
        methodLikeGetInstance = nullptr;      
        friendFunctionLikeGetInstance = nullptr;
        instanceField = nullptr;
        hiddenInstanceMethod = false;
        ctorsPrivate = true;
        hasMethodLikelyInstance = false;
        hasDeletedCopyConstuctor = false;
        hasDeletedAssigmentOperator = false;
        isSingltone = true;
        amountObjects = 0;
        probabalyNaiveSingletone = false;
        hasFriendFunctionLikelyInstance = false;
        unknownPatternSingletone = false;
        SM = nullptr;
        className.clear();
        location.clear();
        
    }

    inline void dump() const noexcept  
    {
        const int totalWidth = 90;
        const int labelWidth = 60;
        
        auto printLine = [](const std::string& text) {
            llvm::outs() << "║ " << text << "\n";
        };
        
        auto printField = [&](const std::string& label, const std::string& value, bool highlight = false) {
            std::string line = "│   • " + label + ":";
            line.resize(labelWidth, ' ');
            line += value;
            if (highlight) {
                line += " ⚡";
            }
            printLine(line);
        };
        
        auto printSection = [&](const std::string& title) {
            std::string line = "│ " + title;
            printLine(line);
        };
        
        auto printSubSection = [&](const std::string& title) {
            std::string line = "│   ─ " + title;
            printLine(line);
        };
        
        auto getAccessString = [](AccessSpecifier access) -> std::string {
            switch (access) {
                case AS_public: return "public";
                case AS_private: return "private";
                case AS_protected: return "protected";
                case AS_none: return "none";
                default: return "unknown";
            }
        };
        
        auto getConditionPatternString = [](ConditionPatternInGetInstance pattern) -> std::string {
            switch (pattern) {
                case UnaryOperatorInCondition: return "Unary Operator (e.g., !instance)";
                case BinaryOperatorInConditionNullptr: return "Binary Operator (e.g., instance == nullptr)";
                case BinaryOperatorInConditionNull: return "Binary Operator (e.g., instance == NULL)";
                case VarInCondition: return "Variable directly in condition";
                case UnknownCondition: return "Unknown condition pattern";
                default: return "Not analyzed";
            }
        };

        llvm::outs() << "\n";
        llvm::outs() << "╔══════════════════════════════════════════════════════════════════════════════════════╗\n";
        llvm::outs() << "║                           SINGLETON PATTERN ANALYSIS REPORT                          ║\n";
        llvm::outs() << "╠══════════════════════════════════════════════════════════════════════════════════════╣\n";
        
        // Basic Class Information
        printLine("│ 📋 CLASS INFORMATION");
        printField("Class Name", className);
        printField("Location", location);
        
        llvm::outs() << "╠══════════════════════════════════════════════════════════════════════════════════════╣\n";
        printLine("│ 🔍 SINGLETON PATTERN ANALYSIS");
        
        // Core Singleton Requirements
        printSection("Core Requirements:");
        printField("Private Constructors", ctorsPrivate ? " ✓ YES" : " ✗ NO", ctorsPrivate);
        printField("Deleted Copy Constructor", hasDeletedCopyConstuctor ? " ✓ YES" : " ✗ NO", hasDeletedCopyConstuctor);
        printField("Deleted Assignment Operator", hasDeletedAssigmentOperator ? " ✓ YES" : " ✗ NO", hasDeletedAssigmentOperator);
        printField("Static Instances Count", std::to_string(amountObjects), amountObjects == 1);
        
        // GetInstance Method Analysis
        printSection("GetInstance Method Analysis:");
        printField("GetInstance Method Found", hasMethodLikelyInstance ? " ✓ YES" : " ✗ NO", hasMethodLikelyInstance);
        
        if (hasMethodLikelyInstance && methodLikeGetInstance) {
            printField("  Method Name", methodLikeGetInstance->getNameAsString());
            printField("  Method Access", getAccessString(methodLikeGetInstance->getAccess()));
            printField("  Method Location", methodLikeGetInstance->getLocation().printToString(*SM));
            printField("  Method Hidden", hiddenInstanceMethod ? " ✓ YES" : " ✗ NO");
            
            if (methodLikeGetInstance->hasBody()) {
                printField("  Has Method Body", " ✓ YES");
            }
        }
        
        // Friend Function Analysis
        printSection("Friend Function Analysis:");
        printField("Friend GetInstance Function", hasFriendFunctionLikelyInstance ? " ✓ YES" : " ✗ NO", 
                   hasFriendFunctionLikelyInstance);
        
        if (hasFriendFunctionLikelyInstance && friendFunctionLikeGetInstance) {
            printField("  Friend Function Name", friendFunctionLikeGetInstance->getNameAsString());
            printField("  Friend Function Location", 
                       friendFunctionLikeGetInstance->getLocation().printToString(*SM));
        }
        
        // Instance Field Analysis
        printSection("Instance Field Analysis:");
        if (instanceField) {
            printField("Instance Field Found", " ✓ YES", true);
            printField("  Field Name", instanceField->getNameAsString());
            printField("  Field Type", instanceField->getType().getAsString());
            printField("  Field Access", getAccessString(instanceField->getAccess()));
            printField("  Field Location", instanceField->getLocation().printToString(*SM));
            printField("  Is Static", instanceField->isStaticDataMember() ? " ✓ YES" : " ✗ NO");
            printField("  Is Static Local", instanceField->isStaticLocal() ? " ✓ YES" : " ✗ NO");
        } else {
            printField("Instance Field Found", " ✗ NOT FOUND");
        }
        
        // Pattern Detection
        printSection("Singleton Pattern Detection:");
        printField("Probably Naive Singleton", probabalyNaiveSingletone ? " ✓ DETECTED" : " ✗ NOT DETECTED", 
                   probabalyNaiveSingletone);
        printField("Probably Mayer's Singleton", probablyMayersSingletone ? " ✓ DETECTED" : " ✗ NOT DETECTED", 
                   probablyMayersSingletone);
        printField("Probably CRTP Singleton", probabalyCRTPSingletone ? " ✓ DETECTED" : " ✗ NOT DETECTED", 
                   probabalyCRTPSingletone);
        printField("Probably If-Naive Singleton", probablyIfNaiveSingletone ? " ✓ DETECTED" : " ✗ NOT DETECTED", 
                   probablyIfNaiveSingletone);
        printField("Probably Flags-Naive Singleton", probablyFlagsNaiveSingletone ? " ✓ DETECTED" : " ✗ NOT DETECTED", 
                   probablyFlagsNaiveSingletone);
        printField("Unknown Pattern Singleton", unknownPatternSingletone ? " ⚠ DETECTED" : " ✗ NOT DETECTED");
        
        // Condition Pattern Analysis
        printSection("Condition Pattern in GetInstance:");
        printField("Condition Pattern", getConditionPatternString(conditionPatternInGetInstance));
        
        // Assignment in If Analysis
        if (assignmentInIfSinglton) {
            printSection("Assignment in If Statement:");
            printField("Assignment Found", " ✓ DETECTED", true);
            printField("  Assignment Location", assignmentInIfSinglton->getBeginLoc().printToString(*SM));
            printField("  Operator", "BO_Assign");
        }
        
        // Final Conclusion
        llvm::outs() << "╠══════════════════════════════════════════════════════════════════════════════════════╣\n";
        printLine("│ 🎯 FINAL CONCLUSION");
        
        std::string conclusion;
        std::string conclusionIcon;
        
        if (isSingltone) {
            conclusion = " ✓ LIKELY SINGLETON PATTERN DETECTED";
            conclusionIcon = "✅";
        } else {
            conclusion = " ✗ NOT A SINGLETON PATTERN";
            conclusionIcon = "❌";
        }
        
        printLine("│ " + conclusionIcon + conclusion);
        
        // Additional pattern details
        if (isSingltone) {
            printLine("│");
            printLine("│ 📝 DETECTED PATTERN DETAILS:");
            
            if (probabalyNaiveSingletone) {
                printLine("│   • Naive Singleton: Static instance field with lazy initialization");
            }
            if (probablyMayersSingletone) {
                printLine("│   • Meyer's Singleton: Static local variable in GetInstance method");
            }
            if (probabalyCRTPSingletone) {
                printLine("│   • CRTP Singleton: Curiously Recurring Template Pattern implementation");
            }
            if (probablyIfNaiveSingletone) {
                printLine("│   • If-Naive Singleton: Conditional initialization in GetInstance");
            }
            if (probablyFlagsNaiveSingletone) {
                printLine("│   • Flags-Naive Singleton: Boolean flag-based initialization control");
            }
            if (unknownPatternSingletone) {
                printLine("│   • Unknown Pattern: Custom singleton implementation detected");
            }
        }
        
        llvm::outs() << "╚══════════════════════════════════════════════════════════════════════════════════════╝\n";
        llvm::outs() << "\n";
    }

};

class GetInstancePatternAnalyser
{
    AnalysisData& analysisData;

    template<typename T1, typename T2>
    struct AnalysisPair 
    {
        T1 extracted;
        T2 param;
    };

    AnalysisPair<VarDecl*, AnalysisData::ConditionPatternInGetInstance> analysisCondition(Expr* ce)
    {
        using AnalysisAlgorithm::getVarDeclFromExpr;
        if (!ce) return {nullptr, AnalysisData::UnknownCondition};
        
        Expr* clearCE = ce->IgnoreParenImpCasts();
        
        // instance
        if (VarDecl* var = getVarDeclFromExpr(clearCE))
            return {var, AnalysisData::VarInCondition};
        
        //  !instance
        else if (auto* unOp = dyn_cast<UnaryOperator>(clearCE)) {
            if (unOp->getOpcode() == UO_LNot) {
                Expr* subExpr = unOp->getSubExpr()->IgnoreParenImpCasts();
                    return {getVarDeclFromExpr(subExpr), AnalysisData::UnaryOperatorInCondition};
            }
        }
        
        // (instance == nullptr, nullptr == instance и т.д.)
        else if (auto* binOp = dyn_cast<BinaryOperator>(clearCE)) {
            if (binOp->getOpcode() == BO_EQ || binOp->getOpcode() == BO_NE) {
                Expr* lhs = binOp->getLHS()->IgnoreParenImpCasts();
                Expr* rhs = binOp->getRHS()->IgnoreParenImpCasts();
                
                auto checkNullComparison = [&](Expr* varSide, Expr* nullSide) -> VarDecl* {
                    if (VarDecl* var = getVarDeclFromExpr(varSide))
                        if (isa<CXXNullPtrLiteralExpr>(nullSide) 
                        ||  isa<GNUNullExpr>(nullSide)) 
                                return var;
                    return nullptr;
                };
                
                if (VarDecl* var = checkNullComparison(lhs, rhs)) {
                    return {var, AnalysisData::BinaryOperatorInConditionNullptr};
                }
                if (VarDecl* var = checkNullComparison(rhs, lhs)) {
                    return {var, AnalysisData::BinaryOperatorInConditionNullptr};
                }
            }
        }
        
        //  (instance ? ... : ...)
        else if (auto* condOp = dyn_cast<ConditionalOperator>(clearCE)) 
            return analysisCondition(condOp->getCond());
        
        return {nullptr, AnalysisData::UnknownCondition};
    }

    VarDecl* analyzeReturnExpression(Expr* retExpr) {
        using AnalysisAlgorithm::getVarDeclFromExpr;
        using AnalysisAlgorithm::extractVarFromUnary;

        if (!retExpr) return nullptr;
        
        retExpr = retExpr->IgnoreParenImpCasts();
       
        // Handle unary operators (& and *) or just var
        VarDecl* returnedVar = extractVarFromUnary(retExpr);
        
        // Handle conditional operator (?:)
        if (auto* condOp = dyn_cast<ConditionalOperator>(retExpr)) {
            returnedVar = analyzeConditionalOperator(condOp);
        }
        
        return returnedVar;
    }
    
    VarDecl* analyzeConditionalOperator(ConditionalOperator* condOp) {
        using AnalysisAlgorithm::getVarDeclFromExpr;
        using AnalysisAlgorithm::extractVarFromUnary;

        auto conditionResult = analysisCondition(condOp->getCond());
        VarDecl* conditionVar = conditionResult.extracted;
        
        if (!conditionVar) {
            analysisData.unknownPatternSingletone = true;
            return nullptr;
        }

        VarDecl* returnedVar = extractVarFromUnary(condOp->getTrueExpr()) ? : extractVarFromUnary(condOp->getFalseExpr());
        
        if (returnedVar != conditionVar) {
            if (conditionVar->getType()->isBooleanType()) {
                analysisData.probablyFlagsNaiveSingletone = true;
            } else {
                analysisData.unknownPatternSingletone = true;
            }
        }
        
        return returnedVar;
    }
    
    void analyzeReturnStatement(ReturnStmt* retStmt) {
        using AnalysisAlgorithm::getVarDeclFromExpr;
        
        Expr* retExpr = retStmt->getRetValue();
        if (!retExpr) return;
        
        VarDecl* returnedVar = analyzeReturnExpression(retExpr);
        
        if (returnedVar) {
            analysisData.instanceField = returnedVar;
            // Mayer's pattern
            if (returnedVar->isStaticLocal()) {
                analysisData.probablyMayersSingletone = true;
            }
            // Naive pattern
            else if (returnedVar->isStaticDataMember() && 
                    returnedVar->getAccess() != AS_public) {
                analysisData.probabalyNaiveSingletone = true;
            }
        }
    }
    
    void analyzeIfStatement(IfStmt* ifStmt) {
        using AnalysisAlgorithm::getVarDeclFromExpr;
        using AnalysisAlgorithm::findAssignmentsInStmt;
        
        Expr* condition = ifStmt->getCond();
        if (!condition) return;
        
        auto conditionResult = analysisCondition(condition);
        VarDecl* conditionVar = conditionResult.extracted;
        
        if (!conditionVar) return;
        
        Stmt* thenBody = ifStmt->getThen();
        if (!thenBody) return;
       
        analysisData.probablyFlagsNaiveSingletone = conditionVar->getType()->isBooleanType();

        std::vector<BinaryOperator*> assignments;
        findAssignmentsInStmt(thenBody, assignments);
        
        for (auto* assign : assignments) {
            if (assign->getOpcode() == BO_Assign) {
                VarDecl* assignedVar = getVarDeclFromExpr(assign->getLHS());
                if (assignedVar && assignedVar == conditionVar) {
                    if (assignedVar->isStaticDataMember() && 
                        assignedVar->getAccess() == AS_private) {
                        analysisData.instanceField = assignedVar;
                        analysisData.probablyIfNaiveSingletone = true;
                        analysisData.assignmentInIfSinglton = assign;
                        break;
                    }
                }
            }
        }
    }

    bool isValidSingletonMethodSignature(FunctionDecl *method) {
        return method && method->hasBody() && 
           (method->getReturnType()->isPointerType() || 
            method->getReturnType()->isReferenceType());
    }
public:
    bool isProbablyGetInstanceFunction(FunctionDecl *method) 
    {  
        if (!isValidSingletonMethodSignature(method))
            return false;

        for (Stmt* stmt : method->getBody()->children()) {
            if (!stmt) continue;
            
            if (auto *retStmt = dyn_cast<ReturnStmt>(stmt)) {
                analyzeReturnStatement(retStmt);
            }
            else if (auto* ifStmt = dyn_cast<IfStmt>(stmt)) {
                analyzeIfStatement(ifStmt);
            }
        }
        
        return analysisData.probabalyNaiveSingletone || 
               analysisData.probablyMayersSingletone;
    }

    GetInstancePatternAnalyser( AnalysisData& andata ) : analysisData(andata) {}
};

class ClassVisitor : public RecursiveASTVisitor<ClassVisitor> {
private:
    ASTContext *Context;
    SourceManager* SM;

    AnalysisData analysisData;
    GetInstancePatternAnalyser getInstancePatternAnalyser;

    friend class FunctionVisitor;

private:
        void registerClassForAnalysisData(CXXRecordDecl* clsAST) 
        {
            analysisData.className = clsAST->getNameAsString();
            analysisData.location = clsAST->getLocation().printToString(Context->getSourceManager());
            analysisData.SM = &Context->getSourceManager();
        }

        bool shouldSkipDeclaration(Decl *decl) 
        {
            if (!decl) return true;
            
            SourceLocation loc = decl->getLocation();
            if (!SM->isInMainFile(loc) && !SM->isWrittenInMainFile(loc)) {
                return true;
            }
            
            if (SM->isInSystemHeader(loc) || SM->isInSystemMacro(loc)) {
                return true;
            }
            
            return false;
        }
public:
    explicit ClassVisitor(ASTContext *Context) : Context(Context), getInstancePatternAnalyser(analysisData) {
        SM = &Context->getSourceManager();
    }

    void updateFriendGetInstanceCandidate(FunctionDecl* funcFriend) {
        if (!analysisData.hasFriendFunctionLikelyInstance) {
            analysisData.hasFriendFunctionLikelyInstance = getInstancePatternAnalyser.isProbablyGetInstanceFunction(funcFriend);
            if (analysisData.hasFriendFunctionLikelyInstance) {
                analysisData.friendFunctionLikeGetInstance = funcFriend;
            }
        }
    }

    template<typename T>
    void checkObjectViolations(CXXRecordDecl* declaration, T* decl) {
        using namespace AnalysisAlgorithm;
        
        analysisData.amountObjects += countClassStaticObject(declaration, decl);
        
        if (analysisData.amountObjects > 1 || 
            findClassLocalObject(declaration, decl)) {
            analysisData.isSingltone = false;
        }
    }
    int countClassStaticObjectInExternal(CXXRecordDecl* targetClass, 
                                         CXXRecordDecl* currentClass = nullptr) {
        using AnalysisAlgorithm::countClassStaticObject;
        int count = 0;
        
        if (!currentClass) 
            currentClass = targetClass;
        
        
        return count;
    }

    bool VisitCXXRecordDecl(CXXRecordDecl *declaration) {
       
        using namespace AnalysisAlgorithm;

        if (shouldSkipDeclaration(declaration))
            return true;

        //declaration->dump();
        if (declaration->isEmbeddedInDeclarator() && !declaration->isFreeStanding()) {
            return true;
        }

        if (declaration->getFriendObjectKind() != Decl::FOK_None) {
            return true;
        }
      
        if (!declaration->isThisDeclarationADefinition())
            return true;

        analysisData.clear();
        registerClassForAnalysisData(declaration);

       
        // first stage of analysis
        for (const auto* c : declaration->ctors()) {
            if (c->getAccess() == AS_public && !c->isDeleted()) {
               analysisData.ctorsPrivate = false; 
               break;
            }
        }
        if (!analysisData.ctorsPrivate)
            return true;
        
        analysisData.hasDeletedCopyConstuctor = true;
        analysisData.hasDeletedAssigmentOperator = true;
        for (auto *method : declaration->methods()) {
            if (method->isStatic() && !analysisData.hasMethodLikelyInstance) { 
                analysisData.hasMethodLikelyInstance = getInstancePatternAnalyser.isProbablyGetInstanceFunction(method); 
                if (analysisData.hasMethodLikelyInstance) {
                    analysisData.hiddenInstanceMethod = (method->getAccess() != AS_public); 
                    analysisData.probabalyCRTPSingletone =  method->getReturnType()->isDependentType();
                }
                analysisData.hasMethodLikelyInstance &= compareReturnTypeWithRecordType(method, declaration)
                                                     || analysisData.probabalyCRTPSingletone;
            }
                
            if (CXXConstructorDecl* ctrDecl = dyn_cast<CXXConstructorDecl>(method))
            {
                if (ctrDecl->isCopyOrMoveConstructor()) 
                    analysisData.hasDeletedCopyConstuctor &= ctrDecl->isDeleted();
            }

            if (method->isCopyAssignmentOperator())
                analysisData.hasDeletedAssigmentOperator &= method->isDeleted();
        
            // second stage of analysis
            analysisData.amountObjects += countClassStaticObject(declaration, method);
            if (analysisData.amountObjects > 1 || findClassLocalObject(declaration, method))
                analysisData.isSingltone = false;
        }


        // second stage of analysis  
        if (analysisData.isSingltone) {
            for (auto* field : declaration->decls()) {
                if (isClassObject(dyn_cast<VarDecl>(field), declaration)) { 
                    if (++analysisData.amountObjects > 1) {
                        analysisData.isSingltone = false;
                        break;
                    }
                }
            }
        }
        // third stage of analysis
        if (analysisData.isSingltone) {
            for (FriendDecl* friendDecl : declaration->friends()) {
                if (!analysisData.isSingltone) break;
               
                if (NamedDecl* nd = friendDecl->getFriendDecl()) {
                    if (FunctionDecl* funcFriend = dyn_cast<FunctionDecl>(nd)) {
                        if (compareReturnTypeWithRecordType(funcFriend, declaration)) 
                           updateFriendGetInstanceCandidate(funcFriend); 
                        checkObjectViolations(declaration, funcFriend);    
                    }
                }
                else if (TypeSourceInfo* tsi = friendDecl->getFriendType()) {
                    QualType qt = tsi->getType();
                    if (const RecordType* rt = qt->getAs<RecordType>()) {
                        if (CXXRecordDecl* friendClss = dyn_cast<CXXRecordDecl>(rt->getDecl())) { 
                            checkObjectViolations(declaration, friendClss);
                            for (CXXMethodDecl* friendMethod : friendClss->methods())
                                updateFriendGetInstanceCandidate(friendMethod);
                        }
                    }
                }
            }
        }
        analysisData.isSingltone &= analysisData.probabalyCRTPSingletone 
                                || analysisData.probabalyNaiveSingletone 
                                || analysisData.probablyMayersSingletone 
                                || analysisData.probabalyNaiveSingletone;
        if (analysisData.isSingltone)
            analysisData.dump();
        
        return true;
    }

};

class FunctionVisitor : public RecursiveASTVisitor<FunctionVisitor> {
private:
   ASTContext *Context;
   AnalysisData analysisData;
   GetInstancePatternAnalyser getInstancePatternAnalyser;

   void printInfoFunc(FunctionDecl *func) 
   {
        if (!func) return;
        SourceManager* SM = &Context->getSourceManager(); 
        
        llvm::outs() << "\n";
        llvm::outs() << "╔══════════════════════════════════════════════════════════════════╗\n";
        llvm::outs() << "║                     FUNCTION ANALYSIS REPORT                     ║\n";
        llvm::outs() << "╠══════════════════════════════════════════════════════════════════╣\n";
        
        llvm::outs() << "║ Function: " << func->getNameAsString() << "\n";
        llvm::outs() << "║ Location: " << func->getLocation().printToString(*SM) << "\n";
        llvm::outs() << "║ Return type: " << func->getReturnType().getAsString() << "\n";
        llvm::outs() << "║ Is static: " << (func->isStatic() ? "✓ YES" : "✗ NO") << "\n";
        llvm::outs() << "║ Is global: " << (func->isGlobal() ? "✓ YES" : "✗ NO") << "\n";
        
        llvm::outs() << "╠══════════════════════════════════════════════════════════════════╣\n";
        llvm::outs() << "║ Singleton Pattern Analysis:\n";
    
        if (analysisData.probabalyNaiveSingletone) {
            llvm::outs() << "║ Pattern: Naive Singleton\n";
        } else if (analysisData.probablyMayersSingletone) {
            llvm::outs() << "║ Pattern: Meyer's Singleton\n";
        } else if (analysisData.probablyIfNaiveSingletone) {
            llvm::outs() << "║ Pattern: If-Naive Singleton\n";
        } else if (analysisData.probablyFlagsNaiveSingletone) {
            llvm::outs() << "║ Pattern: Flags-Naive Singleton\n";
        } else {
            llvm::outs() << "║ Pattern: Unknown\n";
        }
        llvm::outs() << "║ • Potential getInstance function ✓ YES" << "\n";
        
        
        llvm::outs() << "╚══════════════════════════════════════════════════════════════════╝\n";
        llvm::outs() << "\n";
    }

public:
    explicit FunctionVisitor(ASTContext *Context) : Context(Context), getInstancePatternAnalyser(analysisData) {}

    bool VisitFunctionDecl(FunctionDecl *func) {
        if (isa<CXXMethodDecl>(func)) {
            return true;
        }
        
        if (!func->hasBody() || !func->getBody()) {
            return true;
        }
        
        if (!func->getReturnType()->isPointerType() 
        &&  !func->getReturnType()->isReferenceType()) {
            return true;
        }
        analysisData.clear();
        if(getInstancePatternAnalyser.isProbablyGetInstanceFunction(func)) {
            printInfoFunc(func);
        }

        return true;
    }
};

class ClassVisitorASTConsumer : public ASTConsumer {
public:
    explicit ClassVisitorASTConsumer(ASTContext *Context) : ClassVisitor(Context), FuncVisitor(Context) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        ClassVisitor.TraverseDecl(Context.getTranslationUnitDecl());
        FuncVisitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    ClassVisitor ClassVisitor;
    FunctionVisitor FuncVisitor;
};

class ClassVisitorPlugin : public PluginASTAction {
public:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                   StringRef InFile) override {
        return std::make_unique<ClassVisitorASTConsumer>(&CI.getASTContext());
    }

    bool ParseArgs(const CompilerInstance &CI,
                  const std::vector<std::string> &args) override {
        for (const auto &Arg : args) {
            if (Arg == "-help") {
                PrintHelp(llvm::errs());
                return false;
            }
        }
        return true;
    }

    void PrintHelp(llvm::raw_ostream &ros) {
        ros << "Class visitor plugin\n";
        ros << "Prints information about classes and their methods\n";
    }
};

} // namespace

static FrontendPluginRegistry::Add<ClassVisitorPlugin>
    X("class-visitor", "Prints information about classes");
