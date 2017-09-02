#include "stdafx.h"
#include "Problem.h"
#include "xmltags.h"
#include "InputData.h"
#include "NetworkArchitecture.h"
#include "Link.h"
#include "Chain.h"
#include "ParameterValues.h"
#include "Network.h"

CProblem::CProblem(tinyxml2::XMLElement * pParentNode)
{
	//load architecture
	tinyxml2::XMLElement *pNode = pParentNode->FirstChildElement(XML_TAG_NETWORK_ARCHITECTURE);
	m_pNetworkArchitecture = CNetworkArchitecture::getInstance(pNode);
	m_pNetworkArchitecture->setParentProblem(this);

	//load inputs
	pNode = pParentNode->FirstChildElement(XML_TAG_Inputs);
	loadChildren<CInputData>(pNode, XML_TAG_InputData, m_inputs);

	//load output
	pNode = pParentNode->FirstChildElement(XML_TAG_Output)->FirstChildElement(XML_TAG_LinkConnection);
	m_output = CLinkConnection::getInstance(pNode);
}

CProblem::CProblem()
{
}


CProblem::~CProblem()
{
}

CProblem * CProblem::getInstance(tinyxml2::XMLElement * pNode)
{
	if (!strcmp(pNode->Name(), XML_TAG_Problem))
		return new CProblem(pNode);

	return nullptr;
}


CNetwork * CProblem::createNetwork()
{
	CNetwork* result = new CNetwork();
	/*
	basic idea of this algorithm: we traverse all chains to create the real model
	to achieve this we follow this pseudo-code:

	while output_node has not been reached:
	for each chain  in chains
	traverse chain as far as possible

	stop the processing when no changes are possible anymore -> the structure is faulty!

	*/

	//create the inputs
	for each (CInputData* input in m_inputs)
	{
		if (input->getInputFunctionPtr() == nullptr)
		{
			input->createInputFunctionPtr();
			result->getInputs().push_back(input);
		}
	}

	CNTK::FunctionPtr pOutputFunction = nullptr;

	auto progressMap = std::map<CChain*, CLink*>();
	CLink* pCurrentLink;
	vector<const CLink*> dependencies;
	bool architectureDeadEndReached = false;
	while (pOutputFunction == nullptr && !architectureDeadEndReached)
	{
		architectureDeadEndReached = true;

		for each (CChain* pChain in m_pNetworkArchitecture->getChains())
		{
			//get current link of the chain
			if (progressMap.find(pChain) == progressMap.end())
			{
				//start at the beginning
				pCurrentLink = pChain->getChainLinks().at(0);
			}
			else
			{
				//resume the progress
				pCurrentLink = progressMap.at(pChain);
			}

			while (pCurrentLink != nullptr)
			{
				//get dependencies
				dependencies = pCurrentLink->getDependencies();

				//check if dependencies have already been traversed
				bool dependencies_satisfied = true;
				for each (const CLink* item in dependencies)
				{
					if (item->getFunctionPtr() == nullptr)
					{
						dependencies_satisfied = false;
						break;
					}
				}

				//dependencies not satisfied -> try to process another chain
				if (!dependencies_satisfied)
					break;

				//all dependencies are satisfied and already traversed
				//create the FunctionPtr now and save it!

				//TODO: add real code
				pCurrentLink->createCNTKFunctionPtr(dependencies);
				result->getFunctionPtrs().push_back(pCurrentLink->getFunctionPtr());

				//pCurrentLink->getFunctionPtr()->Save(L"C:\\Users\\Roland\\Desktop\\graph.dat");

				//check if output has been reached already
				if (!strcmp(m_output->getTargetId().c_str(), pCurrentLink->getId().c_str()))
				{
					pOutputFunction = pCurrentLink->getFunctionPtr()->Output();
					result->getOutputsFunctionPtr().push_back(pOutputFunction);
					break;
				}

				//update current link
				pCurrentLink = pCurrentLink->getNextLink();

				//keep the loop alive
				architectureDeadEndReached = false;

			}

			if (pOutputFunction != nullptr)
				break;

			progressMap.insert_or_assign(pChain, pCurrentLink);
		}
	}

	return result;
}