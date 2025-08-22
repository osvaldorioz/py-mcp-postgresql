<img width="1373" height="832" alt="imagen" src="https://github.com/user-attachments/assets/c35d8662-2dfc-4827-8467-2ab832c42210" />

El programa MCP-SQL es una aplicación diseñada para analizar una base de datos relacional (PostgreSQL) y generar dashboards interactivos basados en métricas extraídas de las tablas de una base de datos postgreSQL. Utiliza una combinación de C++ (para interactuar con la base de datos y procesar datos), Python (para la interfaz de usuario y la API), y un modelo de lenguaje (LLM) para generar consultas SQL y dashboards HTML estilizados. A continuación, se detalla su funcionamiento, estructura, y propósito, manteniendo los mensajes de error en español y el código/configuración en inglés, como se ha establecido.

MCP-SQL permite:

Analizar la base de datos: Identificar métricas clave (KPIs) y generar consultas SQL basadas en el esquema de las tablas.
Extraer datos: Ejecutar consultas SQL para obtener datos reales de las tablas usando JOINs.
Generar dashboards: Crear visualizaciones HTML responsivas con gráficos (usando Chart.js) y tablas, estilizadas con Tailwind CSS.
Interfaz de usuario: Proporcionar una interfaz web (Streamlit) para interactuar con el sistema mediante prompts en lenguaje natural.

Estructura del Programa
El programa se organiza en los siguientes componentes principales:

cpp_agent.cpp:

Lenguaje: C++ con bibliotecas pqxx (PostgreSQL), nlohmann/json (manejo de JSON), y pybind11 (integración con Python).
Funcionalidades:

run_agent: Procesa prompts del usuario, interactúa con el LLM, y usa herramientas (get_schema, read_query) para obtener el esquema de la base de datos o ejecutar consultas SQL.
analyze_database: Analiza el esquema y sugiere métricas con consultas SQL en formato JSON.
get_data_from_database: Ejecuta consultas SQL (usando read_query) y devuelve datos en formato JSON.
generate_html_dashboard: Genera un dashboard HTML con Chart.js y Tailwind CSS a partir de datos JSON. Si el LLM falla, genera un dashboard predeterminado con gráficos y tablas basados en datos reales.


Compilación: Se compila en un módulo compartido (cpp_agent.so) para su uso en Python.


config.json:

Formato: JSON en inglés.
Contenido: Contiene instrucciones para el LLM:

- INSTRUCTIONS: Guía para responder prompts con resúmenes en lenguaje natural.
- VISUALIZATION_TYPES_JSON: Define tipos de visualización (por ejemplo, bar_chart, table).
- INSTRUCTIONS_DB_ANALYSIS_AND_SQL: Instrucciones para analizar el esquema y sugerir métricas.
- INSTRUCTIONS_SQL_METRIC_DATA_JSON_ONLY: Instrucciones para ejecutar consultas SQL y devolver datos en JSON.
- INSTRUCTIONS_RENDER_DASHBOARD_FROM_DATA: Instrucciones para generar HTML con Chart.js y Tailwind CSS, usando datos reales.


api.py:

Lenguaje: Python con Uvicorn.
Función: Implementa una API FastAPI (uvicorn) que actúa como intermediario entre la interfaz de usuario y el módulo C++.
Endpoints:

/run_agent: Procesa prompts del usuario y devuelve resúmenes en lenguaje natural.
/run_dashboard_agent: Ejecuta el flujo completo para generar un dashboard (análisis, datos, HTML).


Integración: Usa cpp_agent.so para interactuar con la base de datos y el LLM.


app.py:

Lenguaje: Python con Streamlit.
Función: Proporciona una interfaz web con dos pestañas:

Chatbot: Permite al usuario ingresar prompts en lenguaje natural en español o inglés(por ejemplo, "List all sales for the North region").
Panel de Control: Genera un dashboard HTML descargable basado en un prompt (por ejemplo, "Generate a dashboard showing sales by product").


Interacción: Llama a los endpoints de api.py.

<img width="1375" height="853" alt="imagen" src="https://github.com/user-attachments/assets/6e0343d1-4c8b-4196-9c46-bbb328d655a1" />


Base de Datos:

Motor: PostgreSQL
Tablas usadas en pruebas:

sales: Columnas id, region, sales_amount, sale_date, product_id (FK a products.id).
customers: Columnas id, name, email, sale_id (FK a sales.id).
products: Columnas id, name, price, category.

