import streamlit as st
import requests
import json
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

API_URL = "http://localhost:8000"

st.set_page_config(
    page_title="MCP PostgreSQL",
    page_icon="🐘",
    layout="wide",
    initial_sidebar_state="expanded",
    menu_items={
    }
)

st.title("🐘 MCP PostgreSQL")
st.write(
    "Interactúa con una base de datos SQL usando lenguaje natural y visualiza tendencias de la base de datos. "
    "Esta aplicación se conecta a un servidor SQL MCP para ejecutar tus consultas, mostrar resultados y generar paneles."
)

if "messages" not in st.session_state:
    st.session_state.messages = []
if "dashboard_html" not in st.session_state:
    st.session_state.dashboard_html = ""

tab1, tab2 = st.tabs(["Chatbot", "Dashboard"])

with tab1:
    chat_container = st.container()
    chat_container.markdown('<div id="chat-container"></div>', unsafe_allow_html=True)

    with chat_container:
        for message in st.session_state.messages:
            with st.chat_message(message["role"]):
                st.markdown(message["content"])

    user_query = st.chat_input("Haz una pregunta sobre la base de datos (por ejemplo, 'Listar todas las ventas')", key="chat_input")

    if user_query:
        st.session_state.messages.append({"role": "user", "content": user_query})
        with chat_container:
            with st.chat_message("user"):
                st.markdown(user_query)

        with st.spinner("Obteniendo respuesta de la base de datos..."):
            try:
                logger.info(f"Enviando solicitud GET a {API_URL}/run_agent/{user_query}")
                resp = requests.get(f"{API_URL}/run_agent/{user_query.replace(' ', '%20')}")
                resp.raise_for_status()
                result = resp.json().get("result")
                st.session_state.messages.append({"role": "assistant", "content": result})
                with chat_container:
                    with st.chat_message("assistant"):
                        st.markdown(result)
            except requests.exceptions.HTTPError as e:
                logger.error(f"Error HTTP: {str(e)}")
                st.error(f"Error HTTP: {str(e)}")
            except Exception as e:
                logger.error(f"Error: {str(e)}")
                st.error(f"Error: {str(e)}")

with tab2:
    st.header("Dashboard de la Base de Datos")

    if st.button("Generar un Dashboard"):
        with st.spinner("Analizando la base de datos y generando el Dashboard..."):
            try:
                query = "Analiza mi base de datos y sugiere un panel de control"
                logger.info(f"Enviando solicitud GET a {API_URL}/run_dashboard_agent/{query}")
                resp = requests.get(f"{API_URL}/run_dashboard_agent/{query.replace(' ', '%20')}")
                resp.raise_for_status()
                dashboard_html = resp.json().get("result")
                st.session_state.dashboard_html = dashboard_html
                st.components.v1.html(dashboard_html, height=800, scrolling=True)
            except requests.exceptions.HTTPError as e:
                logger.error(f"Error HTTP: {str(e)}")
                st.error(f"Error HTTP: {str(e)}")
            except Exception as e:
                logger.error(f"Error: {str(e)}")
                st.error(f"Error: {str(e)}")

    if st.session_state.dashboard_html:
        st.download_button(
            label="Descargar HTML del Dashboard",
            data=st.session_state.dashboard_html,
            file_name="dashboard.html",
            mime="text/html",
        )