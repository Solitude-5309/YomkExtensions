# FastDDS 源码编译

## linux下 Fast-DDS-3.6.1 编译步骤

### 安装环境

1. sudo apt install cmake g++ python3-pip wget git

### 安装依赖包

1. sudo apt install libasio-dev libtinyxml2-dev
2. sudo apt install libssl-dev
3. sudo apt install libp11-dev
4. sudo usermod -a -G softhsm $USER
5. sudo apt install libengine-pkcs11-openssl
6. p11-kit list-modules
7. openssl engine pkcs11 -t
8. 可选：git clone --branch release-1.11.0 https://github.com/google/googletest src/googletest-distribution
9. python3 -m pip install --upgrade pip
10. pip install xmlschema

### 创建文件目录

1. mkdir ./Fast-DDS-3.6.1
2. Fast-DDS-3.6.1源码：Fast-DDS-3.6.1/Fast-DDS-3.6.1
3. Fast-CDR-2.3.5源码：Fast-DDS-3.6.1/Fast-CDR-2.3.5
4. foonathan_memory_vendor-1.4.1源码：Fast-DDS-3.6.1/foonathan_memory_vendor-1.4.1

### 编译Foonathan memory
1. cd foonathan_memory_vendor-1.4.1
2. mkdir build
3. cd build
4. cmake .. -DCMAKE_INSTALL_PREFIX=~/Fast-DDS-3.6.1/install -DBUILD_SHARED_LIBS=ON
5. cmake --build . --target install --config Release

### 编译Fast CDR
1. cd Fast-CDR-2.3.5
2. mkdir build
3. cd build
4. cmake .. -DCMAKE_INSTALL_PREFIX=~/Fast-DDS-3.6.1/install
5. cmake --build . --target install --config Release

### 编译Fast DDS
1. cd Fast-DDS-3.6.1
2. mkdir build
3. cd build
4. cmake .. -DCMAKE_INSTALL_PREFIX=~/Fast-DDS-3.6.1/install
5. cmake --build . --target install --config Release

### 编译Fast DDS python
1. cd Fast-DDS-python-2.6.1
2. mkdir -p fastdds_python/build
3. cd fastdds_python/build
4. sudo apt update
5. sudo apt install -y python3 python3-dev python3-pip
6. pip3 install cmake --upgrade
7. export PATH="$HOME/.local/bin:$PATH"
8. cmake .. -DCMAKE_INSTALL_PREFIX=~/Fast-DDS-python-2.6.1/install -DCMAKE_PREFIX_PATH=$HOME/Fast-DDS-3.6.1/install [cmake 3.22]
9. cmake --build . --target install --config Release

#### 编译Fast DDS-Gen

1. sudo apt install openjdk-17-jdk
2. Fast-DDS-Gen-4.3.0目录：Fast-DDS-3.6.1/Fast-DDS-Gen-4.3.0
3. IDL-Parser-4.3.0目录：Fast-DDS-3.6.1/IDL-Parser-4.3.0
4. cd IDL-Parser-4.3.0
5. ./gradlew clean build [换成国内源：distributionUrl=https\://mirrors.cloud.tencent.com/gradle/gradle-9.2.1-bin.zip]
6. cd Fast-DDS-Gen-4.3.0
7. 上述步骤已经手动编译IDL-Parser-4.3.0，因此关闭Fast-DDS-Gen-4.3.0中的更新git子模块，不从git下载IDL-Parser

// task submodulesUpdate(type: Exec) {
//     description = 'Updates (and inits) git submodules'
//     commandLine = ['git', 'submodule', 'update', '--init']
// }

// task buildIDLParser(type: GradleBuild) {
//     dir = 'thirdparty/idl-parser'
//     tasks = ['clean', 'build']
// }
// buildIDLParser.dependsOn submodulesUpdate

compileJava.dependsOn copyResources

8. 拷贝Fast-DDS-3.6.1/IDL-Parser-4.3.0/build/libs/idlparser-4.3.0.jar 到 Fast-DDS-3.6.1/Fast-DDS-Gen-4.3.0/thirdparty/idl-parser/build/libs/idlparser-4.3.0.jar
9. ./gradlew assemble [换成国内源：distributionUrl=https\://mirrors.cloud.tencent.com/gradle/gradle-9.2.1-bin.zip]


## linux下 Fast-DDS-3.6.1 C++使用方法

### 创建example目录

1. example目录：Fast-DDS-3.6.1/examples
2. 拷贝hello_world示例：Fast-DDS-3.6.1/Fast-DDS-3.6.1/examples/cpp/hello_world to Fast-DDS-3.6.1/examples

### 编译示例

1. cd examples/hello_world
2. mkdir build
3. cd build
4. cmake .. -DCMAKE_PREFIX_PATH=$HOME/Fast-DDS-3.6.1/install [修改cmake版本要求为3.22]
5. cmake --build . --config Release

### 运行示例

#### 运行发布者

1. export LD_LIBRARY_PATH=$HOME/Fast-DDS-3.6.1/install/lib:$LD_LIBRARY_PATH
2. ./hello_world publisher

#### 运行订阅者

1. export LD_LIBRARY_PATH=$HOME/Fast-DDS-3.6.1/install/lib:$LD_LIBRARY_PATH
2. ./hello_world subscriber

### 使用Fast-DDS-Gen

1. cd Fast-DDS-3.6.1/Fast-DDS-Gen-4.3.0/scripts
2. 创建一个最简单的idl文件rpc.idl，内容如下

struct RPCString
{
    string s;
};

3. 创建一个gen目录用于存放生成文件
4. 生成C++代码：./fastddsgen -d gen rpc.idl 或 生成python代码：./fastddsgen -python -d gen rpc.idl

## linux下 Fast-DDS-3.6.1 Python使用方法

### 创建example目录

1. example目录：Fast-DDS-3.6.1/examples
2. 拷贝HelloWorldExample示例：Fast-DDS-3.6.1/Fast-DDS-python-2.6.1/fastdds_python_examples/HelloWorldExample to Fast-DDS-3.6.1/examples

### 编译示例
1. 安装swig：sudo apt install swig
2. sudo apt install libpython3-dev
3. cd examples/HelloWorldExample/generated_code
4. mkdir build
5. cd build
6. cmake .. -DCMAKE_PREFIX_PATH=$HOME/Fast-DDS-3.6.1/install [修改cmake版本要求为3.22]
7. cmake --build . --config Release

### 运行发布者
1. export LD_LIBRARY_PATH=$HOME/Fast-DDS-3.6.1/install/lib:$LD_LIBRARY_PATH
2. export PYTHONPATH=$HOME/Fast-DDS-python-2.6.1/install/lib/python3.10/site-packages:$PYTHONPATH
3. 将examples/HelloWorldExample/generated_code/build目录下的HelloWorld.py和_HelloWorldWrapper.so拷贝到examples/HelloWorldExample 
4. cd examples/HelloWorldExample
5. python3 HelloWorldExample.py -p publisher

### 运行订阅者
1. export LD_LIBRARY_PATH=$HOME/Fast-DDS-3.6.1/install/lib:$LD_LIBRARY_PATH
2. export PYTHONPATH=$HOME/Fast-DDS-python-2.6.1/install/lib/python3.10/site-packages:$PYTHONPATH
3. 将examples/HelloWorldExample/generated_code/build目录下的HelloWorld.py和_HelloWorldWrapper.so拷贝到examples/HelloWorldExample 
4. cd examples/HelloWorldExample
5. python3 HelloWorldExample.py -p subscriber

### 编译python idl

1. 安装swig：sudo apt install swig
2. sudo apt install libpython3-dev
3. cd gen
4. mkdir build
5. cd build
6. cmake .. -DCMAKE_PREFIX_PATH=$HOME/Fast-DDS-3.6.1/install
7. cmake --build . --config Release


