from fastapi import FastAPI
import cpp_agent
from openai import AzureOpenAI
import os
import json
import logging
from dotenv import load_dotenv

# Variables desde el archivo .env
load_dotenv()

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI()

# Configuracion Azure OpenAI
AZURE_OPENAI_ENDPOINT = os.getenv("AZURE_OPENAI_ENDPOINT")
AZURE_OPENAI_API_KEY = os.getenv("AZURE_OPENAI_API_KEY")
AZURE_OPENAI_DEPLOYMENT = os.getenv("AZURE_OPENAI_DEPLOYMENT", "gpt-4")
AZURE_OPENAI_API_VERSION = os.getenv("AZURE_OPENAI_API_VERSION", "2024-02-15-preview")

if not all([AZURE_OPENAI_ENDPOINT, AZURE_OPENAI_API_KEY]):
    raise ValueError("Missing Azure OpenAI environment variables")

client = AzureOpenAI(
    azure_endpoint=AZURE_OPENAI_ENDPOINT,
    api_key=AZURE_OPENAI_API_KEY,
    api_version=AZURE_OPENAI_API_VERSION
)

def llm_callback(messages, tools):
    try:
        logger.info(f"Sending request to Azure OpenAI with messages: {messages}")
        response = client.chat.completions.create(
            model=AZURE_OPENAI_DEPLOYMENT,
            messages=messages,
            tools=tools if tools else [],
            tool_choice="auto" if tools else None
        )
        response_json = response.to_dict()
        logger.info(f"Azure OpenAI response: {response_json}")
        return json.dumps({
            "choices": [
                {
                    "message": {
                        "content": response_json["choices"][0]["message"].get("content"),
                        "tool_calls": response_json["choices"][0]["message"].get("tool_calls", [])
                    }
                }
            ]
        })
    except Exception as e:
        logger.error(f"Azure OpenAI error: {str(e)}")
        return json.dumps({"error": {"message": str(e)}})

@app.get("/run_agent/{query}")
async def run_agent(query: str):
    try:
        logger.info(f"Processing query: {query}")
        result = cpp_agent.run_agent(query, llm_callback)
        logger.info(f"Query result: {result}")
        return {"result": result}
    except Exception as e:
        logger.error(f"Error processing query: {str(e)}")
        return {"error": str(e)}

@app.get("/run_dashboard_agent/{query}")
async def run_dashboard_agent(query: str):
    try:
        logger.info(f"Processing dashboard query: {query}")
        result = cpp_agent.run_dashboard_agent(query, llm_callback)
        logger.info(f"Dashboard result length: {len(result)}")
        return {"result": result}
    except Exception as e:
        logger.error(f"Error processing dashboard query: {str(e)}")
        return {"error": str(e)}