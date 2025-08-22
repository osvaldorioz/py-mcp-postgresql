#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <nlohmann/json.hpp>
#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <regex>
#include <fstream>
#include <cstdlib>
#include <iostream>

namespace py = pybind11;
using json = nlohmann::json;

// Convertidor de tipo para nlohmann::json
namespace pybind11::detail {
    template <> struct type_caster<nlohmann::json> {
        PYBIND11_TYPE_CASTER(nlohmann::json, _("nlohmann::json"));
        bool load(handle src, bool) {
            if (!src) return false;
            try {
                if (py::isinstance<py::dict>(src)) {
                    value = py::cast<nlohmann::json>(src);
                } else if (py::isinstance<py::list>(src)) {
                    value = py::cast<nlohmann::json>(src);
                } else if (py::isinstance<py::str>(src)) {
                    value = nlohmann::json::parse(py::cast<std::string>(src));
                } else if (src.is_none()) {
                    value = nlohmann::json();
                } else {
                    return false;
                }
                return true;
            } catch (...) {
                return false;
            }
        }
        static handle cast(nlohmann::json src, return_value_policy /* policy */, handle /* parent */) {
            try {
                if (src.is_object()) {
                    py::dict dict;
                    for (auto& [key, val] : src.items()) {
                        dict[py::str(key)] = cast(val, return_value_policy::automatic, {});
                    }
                    return dict.release();
                } else if (src.is_array()) {
                    py::list list;
                    for (auto& val : src) {
                        list.append(cast(val, return_value_policy::automatic, {}));
                    }
                    return list.release();
                } else if (src.is_string()) {
                    return py::str(src.get<std::string>()).release();
                } else if (src.is_boolean()) {
                    return py::bool_(src.get<bool>()).release();
                } else if (src.is_number_integer()) {
                    return py::int_(src.get<long>()).release();
                } else if (src.is_number_float()) {
                    return py::float_(src.get<double>()).release();
                } else if (src.is_null()) {
                    return py::none().release();
                }
                return py::none().release();
            } catch (...) {
                throw py::cast_error("Error al convertir nlohmann::json a objeto Python");
            }
        }
    };
}

namespace {
// Cargar configuración desde config.json
json load_config() {
    std::ifstream file("config.json");
    if (!file.is_open()) {
        throw std::runtime_error("No se pudo abrir config.json");
    }
    json config;
    try {
        file >> config;
    } catch (const json::parse_error& e) {
        throw std::runtime_error("Error al parsear config.json: " + std::string(e.what()));
    }
    return config;
}

const json CONFIG = load_config();
const std::string INSTRUCTIONS = CONFIG["INSTRUCTIONS"].get<std::string>();
const std::string VISUALIZATION_TYPES_JSON = CONFIG["VISUALIZATION_TYPES_JSON"].dump();
const std::string INSTRUCTIONS_DB_ANALYSIS_AND_SQL = CONFIG["INSTRUCTIONS_DB_ANALYSIS_AND_SQL"].get<std::string>() + "\nTipos de visualización: " + VISUALIZATION_TYPES_JSON;
const std::string INSTRUCTIONS_SQL_METRIC_DATA_JSON_ONLY = CONFIG["INSTRUCTIONS_SQL_METRIC_DATA_JSON_ONLY"].get<std::string>();
const std::string INSTRUCTIONS_RENDER_DASHBOARD_FROM_DATA = CONFIG["INSTRUCTIONS_RENDER_DASHBOARD_FROM_DATA"].get<std::string>();

// Cadena de conexión a la base de datos
std::string get_conninfo() {
    std::string host = std::getenv("DB_HOST") ? std::getenv("DB_HOST") : "";
    std::string user = std::getenv("DB_USER") ? std::getenv("DB_USER") : "";
    std::string pw = std::getenv("DB_PASSWORD") ? std::getenv("DB_PASSWORD") : "";
    std::string dbname = std::getenv("DB_NAME") ? std::getenv("DB_NAME") : "";
    if (host.empty() || user.empty() || pw.empty() || dbname.empty()) {
        throw std::runtime_error("Faltan variables de entorno de la base de datos");
    }
    return "host=" + host + " user=" + user + " password=" + pw + " dbname=" + dbname;
}

// Obtener el esquema de la base de datos
std::string get_db_schema() {
    try {
        pqxx::connection conn(get_conninfo());
        pqxx::work txn(conn);
        pqxx::result res = txn.exec("SELECT table_name, column_name, data_type FROM information_schema.columns WHERE table_schema = 'public' ORDER BY table_name, ordinal_position;");

        json schema = json::array();
        for (auto row : res) {
            schema.push_back({
                {"table_name", row[0].c_str()},
                {"column_name", row[1].c_str()},
                {"data_type", row[2].c_str()}
            });
        }
        return schema.dump();
    } catch (const std::exception& e) {
        json error = {{"error", std::string("Error al obtener el esquema: ") + e.what()}};
        return error.dump();
    }
}

// Ejecutar consulta
std::string read_db_query(const std::string& query) {
    try {
        if (query.find("SELECT") != 0 && query.find("select") != 0) {
            throw std::runtime_error("Solo se permiten consultas SELECT");
        }
        pqxx::connection conn(get_conninfo());
        pqxx::work txn(conn);
        pqxx::result res = txn.exec(query);

        json results = json::array();
        for (auto row : res) {
            json obj;
            for (auto field : row) {
                if (field.is_null()) {
                    obj[field.name()] = nullptr;
                } else {
                    // Manejar diferentes tipos de datos
                    switch (field.type()) {
                        case 20: // int8
                        case 23: // int4
                            obj[field.name()] = field.as<long>();
                            break;
                        case 700: // float4
                        case 701: // float8
                            obj[field.name()] = field.as<double>();
                            break;
                        default: // Tratar como cadena (por ejemplo, varchar, date)
                            obj[field.name()] = field.as<std::string>();
                            break;
                    }
                }
            }
            results.push_back(obj);
        }
        return results.dump();
    } catch (const std::exception& e) {
        json error = {{"error", std::string("Error en la consulta: ") + e.what()}};
        return error.dump();
    }
}

// Definición de herramientas
json get_tools(bool schema, bool query) {
    json tools = json::array();
    if (schema) {
        tools.push_back({
            {"type", "function"},
            {"function", {
                {"name", "get_schema"},
                {"description", "Recupera el esquema completo de la base de datos."},
                {"parameters", {
                    {"type", "object"},
                    {"properties", json::object()},
                    {"required", json::array()}
                }}
            }}
        });
    }
    if (query) {
        tools.push_back({
            {"type", "function"},
            {"function", {
                {"name", "read_query"},
                {"description", "Ejecuta una consulta SELECT y devuelve el resultado como una lista de diccionarios."},
                {"parameters", {
                    {"type", "object"},
                    {"properties", {
                        {"query", {
                            {"type", "string"},
                            {"description", "La consulta SQL SELECT a ejecutar."}
                        }}
                    }},
                    {"required", {"query"}}
                }}
            }}
        });
    }
    return tools;
}

// Limpiar JSON
std::string clean_json_str(const std::string& data) {
    if (data.empty()) {
        return "{}";
    }
    // Verificar bloque de código JSON
    std::regex code_block(R"(```json\s*([\s\S]*?)\s*```)");
    std::smatch match;
    if (std::regex_search(data, match, code_block)) {
        return match[1].str();
    }
    // Encontrar el primer objeto o arreglo JSON válido
    size_t start = data.find('{');
    if (start == std::string::npos) start = data.find('[');
    if (start == std::string::npos) {
        return "{}"; // Devolver objeto vacío si no se encuentra JSON válido
    }
    // Extraer hasta la llave/corchete de cierre correspondiente
    int brace_count = 0;
    char start_char = data[start];
    char end_char = (start_char == '{') ? '}' : ']';
    for (size_t i = start; i < data.length(); ++i) {
        if (data[i] == start_char) brace_count++;
        if (data[i] == end_char) brace_count--;
        if (brace_count == 0) {
            return data.substr(start, i - start + 1);
        }
    }
    return "{}"; // Devolver objeto vacío si el JSON está incompleto
}

// Validar y reintentar
std::string run_with_retries(std::function<std::string(const json&, const json&)> func, const json& messages, const json& tools, int max_retries = 3) {
    for (int retry = 0; retry < max_retries; ++retry) {
        try {
            std::string result = func(messages, tools);
            if (result.empty()) {
                throw std::runtime_error("Resultado vacío desde la devolución de llamada LLM");
            }
            json parsed = json::parse(clean_json_str(result));
            return parsed.dump();
        } catch (const json::parse_error& e) {
            if (retry == max_retries - 1) {
                throw std::runtime_error("Error al parsear JSON después de reintentos: " + std::string(e.what()));
            }
        } catch (const std::exception& e) {
            if (retry == max_retries - 1) {
                throw std::runtime_error("Fallo después de reintentos: " + std::string(e.what()));
            }
        }
    }
    throw std::runtime_error("Fallo después de reintentos");
}

}  // namespace

std::string run_agent(const std::string& message, py::function llm_callback) {
    try {
        json messages = {
            {{"role", "system"}, {"content", INSTRUCTIONS}},
            {{"role", "user"}, {"content", message}}
        };

        json tools = get_tools(true, true);

        std::string result = llm_callback(messages, tools).cast<std::string>();
        json response = json::parse(clean_json_str(result));

        if (response.contains("error")) {
            throw std::runtime_error(response["error"]["message"].get<std::string>());
        }

        if (!response.contains("choices") || response["choices"].empty()) {
            throw std::runtime_error("No hay opciones en la respuesta del LLM");
        }

        json choice = response["choices"][0];
        json message_response = choice["message"];
        // Asegurar que el mensaje tenga un rol
        message_response["role"] = "assistant";
        messages.push_back(message_response);

        int max_loops = 10;
        while (message_response.contains("tool_calls") && !message_response["tool_calls"].empty() && max_loops > 0) {
            for (const auto& tc : message_response["tool_calls"]) {
                std::string name = tc["function"]["name"].get<std::string>();
                std::string args_str = tc["function"]["arguments"].get<std::string>();
                json args;
                try {
                    args = json::parse(args_str);
                } catch (const json::parse_error& e) {
                    throw std::runtime_error("Error al parsear argumentos de la herramienta: " + std::string(e.what()));
                }

                std::string tool_result;
                if (name == "get_schema") {
                    tool_result = get_db_schema();
                } else if (name == "read_query") {
                    if (!args.contains("query")) {
                        throw std::runtime_error("Falta el argumento de consulta en la llamada a read_query");
                    }
                    std::string query = args["query"].get<std::string>();
                    tool_result = read_db_query(query);
                } else {
                    throw std::runtime_error("Herramienta desconocida: " + name);
                }

                json tool_msg = {
                    {"role", "tool"},
                    {"tool_call_id", tc["id"].get<std::string>()},
                    {"name", name},
                    {"content", tool_result}
                };
                messages.push_back(tool_msg);
            }

            result = llm_callback(messages, tools).cast<std::string>();
            response = json::parse(clean_json_str(result));
            if (response.contains("error")) {
                throw std::runtime_error(response["error"]["message"].get<std::string>());
            }
            if (!response.contains("choices") || response["choices"].empty()) {
                throw std::runtime_error("No hay opciones en la respuesta del LLM");
            }
            choice = response["choices"][0];
            message_response = choice["message"];
            message_response["role"] = "assistant";
            messages.push_back(message_response);
            max_loops--;
        }

        if (message_response.contains("content") && !message_response["content"].is_null()) {
            return message_response["content"].get<std::string>();
        } else {
            throw std::runtime_error("No hay contenido válido en la respuesta final del LLM");
        }
    } catch (const std::exception& e) {
        return "Error en run_agent: " + std::string(e.what());
    }
}

std::string analyze_database(const std::string& message, py::function llm_callback) {
    try {
        json messages = {
            {{"role", "system"}, {"content", INSTRUCTIONS_DB_ANALYSIS_AND_SQL}},
            {{"role", "user"}, {"content", message}}
        };

        json tools = get_tools(true, false);

        auto func = [&](const json& msg, const json& tls) { return llm_callback(msg, tls).cast<std::string>(); };
        return run_with_retries(func, messages, tools);
    } catch (const std::exception& e) {
        return "Error en analyze_database: " + std::string(e.what());
    }
}

std::string get_data_from_database(const std::string& analysis_json, py::function llm_callback) {
    try {
        json messages = {
            {{"role", "system"}, {"content", INSTRUCTIONS_SQL_METRIC_DATA_JSON_ONLY}},
            {{"role", "user"}, {"content", analysis_json}}
        };

        json tools = get_tools(false, true);

        auto func = [&](const json& msg, const json& tls) { return llm_callback(msg, tls).cast<std::string>(); };
        return run_with_retries(func, messages, tools);
    } catch (const std::exception& e) {
        return "Error en get_data_from_database: " + std::string(e.what());
    }
}

std::string generate_html_dashboard(const std::string& data_json, py::function llm_callback) {
    try {
        json messages = {
            {{"role", "system"}, {"content", INSTRUCTIONS_RENDER_DASHBOARD_FROM_DATA}},
            {{"role", "user"}, {"content", data_json}}
        };

        std::string html = llm_callback(messages, json::array()).cast<std::string>();
        std::regex html_block(R"(```html\s*([\s\S]*?)\s*```)");
        std::smatch match;
        if (std::regex_search(html, match, html_block)) {
            html = match[1].str();
        } else {
            // Si no se encuentra un bloque HTML, genera un dashboard predeterminado basado en los datos
            std::string labels = "";
            std::string values = "";
            std::string table_rows = "";
            bool found_data = false;
            try {
                json data = json::parse(data_json);
                if (data.contains("metrics") && data["metrics"].is_array() && !data["metrics"].empty()) {
                    for (const auto& metric : data["metrics"]) {
                        if (metric["visualization_type"] == "bar_chart" && metric["data"].is_array()) {
                            for (const auto& row : metric["data"]) {
                                if (row.contains("name") && row.contains("total_sales")) { // product.name, sum(sales_amount)
                                    labels += "\"" + row["name"].get<std::string>() + "\",";
                                    values += std::to_string(row["total_sales"].get<double>()) + ",";
                                    found_data = true;
                                }
                            }
                        }
                        if (metric["visualization_type"] == "table" && metric["data"].is_array()) {
                            for (const auto& row : metric["data"]) {
                                if (row.contains("name") && row.contains("customer_count")) {
                                    table_rows += "<tr><td class=\\\"p-2\\\">" + row["name"].get<std::string>() +
                                                 "</td><td class=\\\"p-2\\\">" +
                                                 std::to_string(row["customer_count"].get<long>()) + "</td></tr>";
                                    found_data = true;
                                }
                            }
                        }
                    }
                }
            } catch (const json::parse_error& e) {
                std::cerr << "Error parsing data_json: " << e.what() << std::endl;
            }

            if (found_data && !labels.empty()) {
                labels.pop_back(); // Eliminar la última coma
                values.pop_back();
                html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Metrics Dashboard</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.3/dist/chart.umd.js"></script>
</head>
<body class="bg-gray-100 p-4">
    <h1 class="text-2xl font-bold text-center mb-6">Metrics Dashboard</h1>
    <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
        <div class="bg-white p-4 rounded-lg shadow-md">
            <h2 class="text-xl font-semibold">Sales by Product</h2>
            <p class="text-gray-600 mb-4">Total sales amount per product</p>
            <canvas id="salesChart"></canvas>
            <script>
                const ctx = document.getElementById('salesChart').getContext('2d');
                new Chart(ctx, {
                    type: 'bar',
                    data: {
                        labels: [)" + labels + R"(],
                        datasets: [{
                            label: 'Sales by Product ($)',
                            data: [)" + values + R"(],
                            backgroundColor: ['#4CAF50', '#2196F3', '#FF9800', '#F44336']
                        }]
                    },
                    options: {
                        scales: {
                            y: { beginAtZero: true, title: { display: true, text: 'Amount ($)' } },
                            x: { title: { display: true, text: 'Product' } }
                        }
                    }
                });
            </script>
        </div>
        <div class="bg-white p-4 rounded-lg shadow-md">
            <h2 class="text-xl font-semibold">Customer Count by Product</h2>
            <p class="text-gray-600 mb-4">Number of customers per product</p>
            <table class="w-full text-left border-collapse">
                <thead>
                    <tr class="bg-gray-200">
                        <th class="p-2">Product</th>
                        <th class="p-2">Customer Count</th>
                    </tr>
                </thead>
                <tbody>
                    )" + table_rows + R"(
                </tbody>
            </table>
        </div>
    </div>
</body>
</html>
)";
            } else {
                html = "<html><body><h1>Error</h1><p>No se encontraron datos válidos para generar el dashboard. Verifique que las consultas SQL devuelvan datos de las tablas sales, customers y products.</p></body></html>";
            }
        }
        return html;
    } catch (const std::exception& e) {
        std::cerr << "Error en generate_html_dashboard: " << e.what() << std::endl;
        return "<html><body><h1>Error</h1><p>Error en generate_html_dashboard: " + std::string(e.what()) + "</p></body></html>";
    }
}

std::string run_dashboard_agent(const std::string& message, py::function llm_callback) {
    try {
        std::string analysis_json = analyze_database(message, llm_callback);
        std::string data_json = get_data_from_database(analysis_json, llm_callback);
        return generate_html_dashboard(data_json, llm_callback);
    } catch (const std::exception& e) {
        return "<html><body><h1>Error</h1><p>Error en run_dashboard_agent: " + std::string(e.what()) + "</p></body></html>";
    }
}

PYBIND11_MODULE(cpp_agent, m) {
    m.def("run_agent", &run_agent);
    m.def("run_dashboard_agent", &run_dashboard_agent);
}