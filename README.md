# **FuncGDAL**

FuncGDAL提供了一个框架，可以将你的算法同GDAL读取图像的接口统一起来，同时该框架提供内置的多种分块存取策略、支持就地算法、简化输出图像创建，从而使可以使得你的算法可以写的更简单。

## 模板参数说明：
- **typename T:**指定用来存放读取到的原图像数据的数组的数据类型。
- **GDALDataType InputType:**指定在RasterIO调用读取原图像数据时的GDALDataType参数值。
- **typename U = T:**指定用来临时存放输出图像数据的数据类型，默认和输入相同。
- **GDALDataType OutType = GDT_Unknown:**指定在RasterIO调用写入输出图像数据时的GDALDataType参数值，默认和输入图像相同。

## 主要接口说明：
- KFunGDAL(std::string fileinput, std::string output = "")
  构造函数，根据指定的输入图像路径名来创建数据集，输出图像路径名为可选参数，如果不使用就地算法那么应该指定该参数。
- KFunGDAL(GDALDataset * dataset, std::string output = "")   
  构造函数，直接指定已经打开的数据集，输出图像路径名为可选参数，如果不使用就地算法那么应该指定该参数，需要注意的是使用就地算法时给定数据集必须具有写入权限。 
- bool RunFunCore(bool(__cdecl * _PtFunCore)(const T *, U *, int, int, int, int, int), KBlockShape shape = SH_SQUARE, bool beDoInplace = false)
  算法执行过程函数，主动调用指定的仿函数作为图像处理算法，可以指定倾向的分块策略以及是否在使用就地算法。分块策略有三种：方形分块，条形分块（长大于宽），条形分块（宽大于长），分别适用于不同的目标。用户在仿函数中实现自己的算法，各个参数分别为输入图像分块后的数据指针，输出图像数据缓冲区指针，总波段数，当前分块长度，当前分块宽度，当前正在处理的块序号，总分块数。
- bool RunFunCoreN2One(bool(__cdecl * _PtFunCore)(const T * const *, U *, int, int, int, int, int), KBlockShape shape = SH_SQUARE)
  算法执行过程函数，主动调用指定的仿函数作为图像处理算法，可以指定倾向的分块策略，该重载主要是为了支持需要综合多个波段信息进行处理并且最终输出只有一个波段的情况。分块策略有三种：方形分块，条形分块（长大于宽），条形分块（宽大于长），分别适用于不同的目标。用户在仿函数中实现自己的算法，各个参数分别为输入图像分块后的数据指针（多波段），输出图像数据缓冲区指针（单波段），总波段数，当前分块长度，当前分块宽度，当前正在处理的块序号，总分块数。
- void SetMaxBlockSize(long _size)
  设定分块时最长边的最大值，默认为5000，当分块为条形时，较短边是长边的0.6倍
- void SetOutFormat(std::string _format)
  设定输出图像的格式，默认为“GTiff”，需要跟GDAL的要求保持一致
- bool CloseDataSet()
  主动关闭数据集，该类在析构时自动关闭波段，也可以手动关闭

## License
The MIT License