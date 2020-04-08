#include "cusppch.h"
#include"solution.h"
#include"cuspparser.h"

void Solution::init(csref sln,csref proj, csref arch, csref tlset, csref cppDial, csref type,
                    const std::vector<std::string>& links, csref author, bool initGit){
    this->solution_name             =   sln;   
    this->project_name              =   proj;   
    this->architecture              =   arch;   
    this->toolset                   =   tlset;   
    this->cppDialect                =   cppDial;   
    this->kind                      =   type;   
    this->libs                      =   links;   
    this->author_name               =   author;   
    this->projects.emplace_back(project_name,cppDialect,kind,libs);
    try
    {
        std::filesystem::create_directory(solution_name);
    }
    catch(const std::exception&)
    {
        __SET_PATTERN_COL__;
        LOG_ERROR("Failed To Create Solution Directory\n");
        EXIT_EXECUTION;
    }
    generateProjectDirectories(solution_name,project_name);
    auto cuspjsonpath=solution_name+"/"+configuationFile;
    serializeCuspDotJson(cuspjsonpath);
    initGitRepo(initGit);
}
    
    void Solution::deserializeCuspDotJson(){
        std::ifstream inFile(configuationFile);
        if(inFile.is_open()){
            nlohmann::json tree;
            inFile >> tree;
            this->solution_name   =     tree["workspace"];    
            this->architecture    =     tree["architecture"]; 
            this->author_name     =     tree["author"];       
            this->toolset         =     tree["toolset"];      
            this->cppDialect      =     tree["cppdialect"];   
            nlohmann::json projs  =     tree["projects"];
            std::for_each(std::begin(projs),std::end(projs),[&](nlohmann::json& p){
                this->projects.emplace_back(p["projectname"],p["cppdialect"]
                ,p["kind"],p["links"]);
            });
        }
        else{
            __SET_PATTERN_COL__;
            LOG_ERROR("Cusp.json Not Found");
            EXIT_EXECUTION;
        }
    }


    void Solution::addProject(csref newProjectName,
                                csref newProjectKind,
                                const std::vector<std::string>& newProjectLibs,
                                csref newProjectCppDialect){

        if(newProjectKind=="consoleapp"){
            for(const auto& proj: projects){
                if(proj.Kind()=="consoleapp"){
                    __SET_PATTERN_COL__;
                    LOG_ERROR("solution can have only one entry point\n");
                    EXIT_EXECUTION;
                }
            }
        }
        this->projects.emplace_back(newProjectName,newProjectCppDialect,
                                        newProjectKind,newProjectLibs);
        generateNewProjectDirectories(newProjectName);
        serializeCuspDotJson(configuationFile); 
    }


void Solution::serializeCuspDotJson(csref path) const {

    auto tree = this->getPropertiesJson();
    std::ofstream out(path);
    if(out.is_open()){
        out<<tree<<std::endl;
        out.close();
        generatePremakeFiles(std::move(tree));
    }
    else{
        __SET_PATTERN_COL__;
        LOG_ERROR("FAILED TO UPDATE CUSP LOG\n");
        EXIT_EXECUTION;
    }
}

    bool Solution::checkCuspInitPreconditions() const{
        return !std::filesystem::exists(configuationFile);
    }

    void Solution::initGitRepo(bool initGit) const
    {
        if (initGit) {

            if (util::getGitEnvironmentVars()[L"Path"].find(L"Git") != std::string::npos) {
                const std::string cmd = "cd " + this->solution_name + " && git init";
                std::system(cmd.c_str()); //initialised git repository
                std::stringstream stream;
                stream  << "# This File Was Generated By Cusp\n"
                        << ".vs/**\n"
                        << ".vscode/**\n"
                        << "*/bin/**\n"
                        << "*/bin-init/**\n"
                        << "*/build/**\n"
                        << "Makefile\n"
                        << "*.sln\n"
                        << "premake5.lua";
                std::ofstream out(solution_name + "/" + ".gitignore");
                if (out.is_open()) {
                    out << stream.str().c_str();
                    out.close();
                }
                else {
                    __SET_PATTERN_COL__;
                    LOG_ERROR("Failed to create .gitignore\n");
                    EXIT_EXECUTION;
                }
            }
            else {
                __SET_PATTERN_COL__;
                LOG_ERROR("Git was not found in your Path\n");
            }
        }
    }

    void Solution::generatePremakeFiles(nlohmann::json tree) const
    {
        cuspParser parser(std::move(tree));
        parser.generatePremakeFiles();
    }

    void Solution::generatePremakeFiles() const
    {
        auto tree = this->getPropertiesJson();
        cuspParser parser(std::move(tree));
        parser.generatePremakeFiles();
    }

	std::string Solution::getBuildSystem() const
	{
        if (this->toolset == "msc")
            return "msbuild";
        return "make";
	}

    nlohmann::json Solution::getPropertiesJson() const
    {
        nlohmann::json tree;
        tree["workspace"] = this->solution_name;
        tree["architecture"] = this->architecture;
        tree["author"] = this->author_name;
        tree["toolset"] = this->toolset;
        tree["cppdialect"] = this->cppDialect;
        std::for_each(std::begin(projects), std::end(projects), [&](const Project& proj) {
            tree["projects"][proj.Name()] = proj.getTree();
            });
        return tree;
    }

    

    void Solution::generateProjectDirectories(csref path,csref pjName) const{
        try{
            if (!(
                std::filesystem::create_directories(path + '/' + pjName)                    &&
                std::filesystem::create_directories(path + '/' + pjName + '/' + "include")  &&
                std::filesystem::create_directories(path + '/' + pjName + '/' + "src")
                ))
            throw std::runtime_error("");
        }
        catch(const std::exception&)
        {
            __SET_PATTERN_COL__;
            LOG_ERROR("failed to create project directories\n");
            EXIT_EXECUTION;
        }
    }

    void Solution::generateNewProjectDirectories(csref pjName) const{
        try{
            std::filesystem::create_directory(pjName);
            std::filesystem::create_directories(pjName+'/'+"include"  );
            std::filesystem::create_directories(pjName+'/'+"src"      );
        }
        catch(const std::exception& e)
        {
            
            __SET_PATTERN_COL__;
            LOG_ERROR(e.what()+'\n');
            LOG_ERROR("failed to create project directories\n");
            EXIT_EXECUTION;
        }
    }
        
    void Solution::addHeader(csref pjName, csref header) const{
       
        try{
            const std::string filePath = pjName+"/include/"+header;
            std::ofstream out(filePath);
            out<<"#pragma once"<<"\n"; 
            out.close();
        }
        catch(const std::exception&)
        {
            __SET_PATTERN_COL__;
            LOG_WARNING("Failed To add Header File\n");
            EXIT_EXECUTION;
        }
    }

    void Solution::addSourceFile(csref pjName,csref File){
        try
        {
            std::string filePath= pjName+"/src/"+File;
            std::ofstream out(filePath);
            out.close();
        }
        catch(const std::exception&)
        {
            __SET_PATTERN_COL__;
            LOG_WARNING("Failed To add Source File\n");
            EXIT_EXECUTION;
        }
         
    }

    void Solution::addClass(csref projectName, csref className){
        if(std::filesystem::exists(projectName)){
            this->addHeader(projectName,className+".h");
            this->addSourceFile(projectName,className+".cpp");
        }
        else{
            __SET_PATTERN_COL__;
            LOG_WARNING("Project Not Found\n");
        }
    }


    const std::string& Solution::getSolutionName() const{
        return this->solution_name;
    }