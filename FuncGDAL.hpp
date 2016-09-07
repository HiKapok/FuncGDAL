/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//                          The MIT License (MIT)
//                  Copyright (c) 2016 wangchangan@yeah.net
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//
//M*/

#include <string>
#include <typeinfo> 
#include "gdal_priv.h"
#include "gdal.h"

template <typename T, GDALDataType InputType, typename U = T, GDALDataType OutType = GDT_Unknown>
class KFunGDAL
{
public:
	typedef enum {
		SH_Reserved = 0,
		SH_SQUARE = 1,
		SH_HLINE = 2,
		SH_VLINE = 3,
		SH_COUNT = 4
	} KBlockShape;

	KFunGDAL(std::string fileinput, std::string output = "") :m_sFileName(fileinput), m_sOutputName(output), m_pInDataSet(NULL), m_sOutFormat("GTiff")/*, m_tDataBuff(NULL)*/, m_lMaxBlockSize(5000)
	{
		GDALAllRegister();
		CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	
	}
	KFunGDAL(GDALDataset * dataset, std::string output = "") :m_sFileName(""), m_sOutputName(output), m_pInDataSet(dataset), m_sOutFormat("GTiff")/*, m_tDataBuff(NULL)*/, m_lMaxBlockSize(5000){}
	~KFunGDAL()
	{
		if (NULL != m_pInDataSet) GDALClose(m_pInDataSet);
	}

	bool RunFunCore(bool(__cdecl * _PtFunCore)(const T *, U *, int, int, int, int, int), KBlockShape shape = SH_SQUARE, bool beDoInplace = false)
	{
		if (NULL == m_pInDataSet && std::string("") != m_sFileName)
		{
			if (beDoInplace) m_pInDataSet = static_cast<GDALDataset *>(GDALOpen(m_sFileName.c_str(), GA_Update));
			else m_pInDataSet = static_cast<GDALDataset *>(GDALOpen(m_sFileName.c_str(), GA_ReadOnly));
			if (NULL == m_pInDataSet) return false;
		}else if (NULL == m_pInDataSet) return false;

		if (beDoInplace && typeid(T) != typeid(U)) return false;
		if (!beDoInplace && std::string("") == m_sOutputName) return false;
		if (!beDoInplace && m_sFileName == m_sOutputName) return false;

		int iXsize = m_pInDataSet->GetRasterXSize();
		int iYsize = m_pInDataSet->GetRasterYSize();
		int iBand = m_pInDataSet->GetRasterCount();
		int xStep = m_lMaxBlockSize;
		int yStep = m_lMaxBlockSize;
		if (SH_VLINE == shape) xStep = m_lMaxBlockSize * 3 / 5;
		else if (SH_HLINE == shape) yStep = m_lMaxBlockSize * 3 / 5;
		yStep = yStep < 1 ? 1 : yStep;
		xStep = xStep < 1 ? 1 : xStep;
		int xBlocks = iXsize / xStep + 1;
		int yBlocks = iYsize / yStep + 1;

		GDALDataset * pOutDataset = NULL;
		if (false == beDoInplace){
			if (GDT_Unknown == OutType) pOutDataset = CreateOutput(m_sOutFormat.c_str(), m_sOutputName, iXsize, iYsize, iBand, InputType, NULL);
			else pOutDataset = CreateOutput(m_sOutFormat.c_str(), m_sOutputName, iXsize, iYsize, iBand, OutType, NULL);
		}else pOutDataset = m_pInDataSet;

		if (NULL == pOutDataset) return false;

		int iBlockCount = 0;
		T *paData = static_cast<T *>(CPLMalloc(sizeof(T)*xStep*yStep));
		U *paOutData = NULL;
		if (false == beDoInplace){
			paOutData = static_cast<U *>(CPLMalloc(sizeof(U)*xStep*yStep));
		}else paOutData = reinterpret_cast<U *>(paData);

		for (int index = 0; index < iBand; ++index){
			GDALRasterBand *pInRasterBand = m_pInDataSet->GetRasterBand(index + 1);
			GDALRasterBand *pOutRasterBand = pOutDataset->GetRasterBand(index + 1);
			for (int iXB = 0; iXB < xBlocks; ++iXB)
			{
				for (int iYB = 0; iYB < yBlocks; ++iYB)
				{
					int xStart = iXB*xStep;
					int yStart = iYB*yStep;

					int xWidth = xStep;
					int yWidth = yStep;

					if (xStart + xStep > iXsize) xWidth = iXsize - xStart;
					if (yStart + yStep > iYsize) yWidth = iYsize - yStart;

					++iBlockCount;

					CPLErr err = pInRasterBand->RasterIO(GF_Read, xStart, yStart, xWidth, yWidth,
						paData, xWidth, yWidth, InputType, 0, 0);

					if (CE_Failure == err)
					{
						if (false == beDoInplace) GDALClose(pOutDataset);
						if (false == beDoInplace) CPLFree(paOutData);
						CPLFree(paData);
						return false;
					}

					if (!_PtFunCore(paData, paOutData, iBand, xWidth, yWidth, iBlockCount, xBlocks * yBlocks * iBand))
					{
						if (false == beDoInplace) GDALClose(pOutDataset);
						if (false == beDoInplace) CPLFree(paOutData);
						CPLFree(paData);
						return false;
					}

					err = pOutRasterBand->RasterIO(GF_Write, xStart, yStart, xWidth, yWidth,
						paOutData, xWidth, yWidth, OutType, 0, 0);

					if (err == CE_Failure)
					{
						if (false == beDoInplace) GDALClose(pOutDataset);
						if (false == beDoInplace) CPLFree(paOutData);
						CPLFree(paData);
						return false;
					}
					pOutRasterBand->FlushCache();
				}
			}
		}

		if (false == beDoInplace) GDALClose(pOutDataset);
		if (false == beDoInplace) CPLFree(paOutData);
		CPLFree(paData);
		return true;
	}
	bool RunFunCoreN2One(bool(__cdecl * _PtFunCore)(const T * const *, U *, int, int, int, int, int), KBlockShape shape = SH_SQUARE)
	{
		if (NULL == m_pInDataSet && std::string("") != m_sFileName)
		{
			m_pInDataSet = static_cast<GDALDataset *>(GDALOpen(m_sFileName.c_str(), GA_ReadOnly));
			if (NULL == m_pInDataSet) return false;
		}
		else if (NULL == m_pInDataSet) return false;

		int iXsize = m_pInDataSet->GetRasterXSize();
		int iYsize = m_pInDataSet->GetRasterYSize();
		int iBand = m_pInDataSet->GetRasterCount();
		int xStep = m_lMaxBlockSize;
		int yStep = m_lMaxBlockSize;
		if (SH_VLINE == shape) xStep = m_lMaxBlockSize * 3 / 5;
		else if (SH_HLINE == shape) yStep = m_lMaxBlockSize * 3 / 5;
		yStep = yStep < 1 ? 1 : yStep;
		xStep = xStep < 1 ? 1 : xStep;
		int xBlocks = iXsize / xStep + 1;
		int yBlocks = iYsize / yStep + 1;

		if (std::string("") == m_sOutputName) return false;
		if (m_sFileName == m_sOutputName) return false;

		GDALDataset * pOutDataset = NULL;
		if (GDT_Unknown == OutType) pOutDataset = CreateOutput(m_sOutFormat.c_str() , m_sOutputName, iXsize, iYsize, 1, InputType, NULL);
		else pOutDataset = CreateOutput(m_sOutFormat.c_str(), m_sOutputName, iXsize, iYsize, 1, OutType, NULL);
		
		if (NULL == pOutDataset) return false;

		int iBlockCount = 0;
		T **paData = static_cast<T **>(CPLMalloc(sizeof(T *)*iBand));
		for (int index = 0; index < iBand; ++index)
		{
			paData[index] = static_cast<T *>(CPLMalloc(sizeof(T)*xStep*yStep));
		}
		U *paOutData = static_cast<U *>(CPLMalloc(sizeof(U)*xStep*yStep));
		
		GDALRasterBand *pOutRasterBand = pOutDataset->GetRasterBand(1);
		
		for (int iXB = 0; iXB < xBlocks; ++iXB)
		{
			for (int iYB = 0; iYB < yBlocks; ++iYB)
			{
				int xStart = iXB*xStep;
				int yStart = iYB*yStep;

				int xWidth = xStep;
				int yWidth = yStep;

				if (xStart + xStep > iXsize) xWidth = iXsize - xStart;
				if (yStart + yStep > iYsize) yWidth = iYsize - yStart;

				++iBlockCount;

				for (int index = 0; index < iBand; ++index){
					GDALRasterBand *pInRasterBand = m_pInDataSet->GetRasterBand(index + 1);
					CPLErr err = pInRasterBand->RasterIO(GF_Read, xStart, yStart, xWidth, yWidth,
						paData[index], xWidth, yWidth, InputType, 0, 0);
					if (CE_Failure == err)
					{
						GDALClose(pOutDataset);
						CPLFree(paOutData);
						for (int index = 0; index < iBand; ++index) CPLFree(paData[index]);
						CPLFree(paData);
						return false;
					}
				}

				if (!_PtFunCore(paData, paOutData, iBand, xWidth, yWidth, iBlockCount, xBlocks * yBlocks))
				{
					GDALClose(pOutDataset);
					CPLFree(paOutData);
					for (int index = 0; index < iBand; ++index) CPLFree(paData[index]);
					CPLFree(paData);
					return false;
				}

				CPLErr err = pOutRasterBand->RasterIO(GF_Write, xStart, yStart, xWidth, yWidth,
					paOutData, xWidth, yWidth, OutType, 0, 0);

				if (err == CE_Failure)
				{
					GDALClose(pOutDataset);
					CPLFree(paOutData);
					for (int index = 0; index < iBand; ++index) CPLFree(paData[index]);
					CPLFree(paData);
					return false;
				}
				pOutRasterBand->FlushCache();
			}
		}

		GDALClose(pOutDataset);
		CPLFree(paOutData);
		for (int index = 0; index < iBand; ++index) CPLFree(paData[index]);
		CPLFree(paData);
		return true;
	}

	void SetMaxBlockSize(long _size){ m_lMaxBlockSize = _size; }
	void SetOutFormat(std::string _format){ m_sOutFormat = _format; }
	bool CloseDataSet(){
		if (NULL != m_pInDataSet){ GDALClose(m_pInDataSet); m_pInDataSet = NULL; return true; }
		return false;
	}
private:
	std::string m_sFileName;
	std::string m_sOutputName;
	GDALDataset *m_pInDataSet;
	std::string m_sOutFormat;
	long m_lMaxBlockSize;
	static GDALDataset *CreateCopyOutput(std::string sDriver, std::string sFileName, GDALDataset *pInDataSet)
	{
		GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName(sDriver.c_str());

		if (NULL == poDriver) return NULL;

		char **papszMetadata = GDALGetMetadata(poDriver, NULL);
		if (!CSLFetchBoolean(papszMetadata, GDAL_DCAP_CREATECOPY, FALSE)) return NULL;
		GDALDataset *pDataset = poDriver->CreateCopy(sFileName.c_str(), pInDataSet, FALSE, NULL, NULL, NULL);
		if (pDataset == NULL) return NULL;
		return pDataset;
	}

	static GDALDataset *CreateOutput(std::string sDriver, std::string sFileName, int nXSize, int nYSize, int nBands, GDALDataType eType, char **arrOptions)
	{
		GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName(sDriver.c_str());

		if (poDriver == NULL) return NULL;

		char **papszMetadata = GDALGetMetadata(poDriver, NULL);
		if (!CSLFetchBoolean(papszMetadata, GDAL_DCAP_CREATE, FALSE)) return NULL;

		GDALDataset *pDataset = poDriver->Create(sFileName.c_str(), nXSize, nYSize, nBands, eType, arrOptions);
		if (pDataset == NULL) return NULL;
		return pDataset;
	}
};

