package com.hermes.wrapper.demo;

import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;

import com.hermes.wrapper.HermesContext;
import com.hermes.wrapper.JSObject;
import com.hermes.wrapper.JSArray;
import com.hermes.wrapper.JSFunction;
import com.hermes.wrapper.JSCallFunction;
import com.hermes.wrapper.android.HermesLoader;

public class MainActivity extends AppCompatActivity {

    private HermesContext hermesContext;
    private EditText inputField;
    private TextView outputView;
    private ScrollView scrollView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Initialize Hermes
        HermesLoader.init(getApplicationContext());
        hermesContext = new HermesContext();

        inputField = findViewById(R.id.input_field);
        outputView = findViewById(R.id.output_view);
        scrollView = findViewById(R.id.scroll_view);

        // Run button
        Button runButton = findViewById(R.id.btn_run);
        runButton.setOnClickListener(v -> executeCode());

        // Demo buttons - row 1
        findViewById(R.id.btn_demo_basic).setOnClickListener(v -> runBasicDemo());
        findViewById(R.id.btn_demo_function).setOnClickListener(v -> runFunctionDemo());
        findViewById(R.id.btn_demo_array).setOnClickListener(v -> runArrayDemo());
        findViewById(R.id.btn_demo_object).setOnClickListener(v -> runObjectDemo());

        // Demo buttons - row 2 (new API tests)
        findViewById(R.id.btn_demo_set_property).setOnClickListener(v -> runSetPropertyDemo());
        findViewById(R.id.btn_demo_get_property).setOnClickListener(v -> runGetPropertyDemo());
        findViewById(R.id.btn_demo_callback).setOnClickListener(v -> runCallbackDemo());
        findViewById(R.id.btn_demo_jsarray).setOnClickListener(v -> runJSArrayDemo());

        // Test Plan button
        findViewById(R.id.btn_test_plan).setOnClickListener(v -> {
            startActivity(new Intent(this, TestPlanActivity.class));
        });

        appendOutput("Hermes Engine initialized!\n");
        appendOutput("Enter JavaScript code and tap Run.\n\n");
    }

    private void executeCode() {
        String code = inputField.getText().toString().trim();
        if (code.isEmpty()) {
            appendOutput("[Error] No code entered\n");
            return;
        }

        try {
            long start = System.currentTimeMillis();
            String result = hermesContext.eval(code);
            long elapsed = System.currentTimeMillis() - start;
            appendOutput("> " + code.split("\n")[0] + (code.contains("\n") ? "..." : "") + "\n");
            appendOutput(result + " (" + elapsed + "ms)\n\n");
        } catch (Exception e) {
            appendOutput("[Error] " + e.getMessage() + "\n\n");
        }
    }

    // ==================== JS-only demos ====================

    private void runBasicDemo() {
        String js = "// Basic calculations\n" +
            "var a = 10;\n" +
            "var b = 20;\n" +
            "var sum = a + b;\n" +
            "var product = a * b;\n" +
            "'Sum: ' + sum + ', Product: ' + product;";
        inputField.setText(js);
        executeCode();
    }

    private void runFunctionDemo() {
        String js = "// Function demo\n" +
            "function fibonacci(n) {\n" +
            "    if (n <= 1) return n;\n" +
            "    return fibonacci(n - 1) + fibonacci(n - 2);\n" +
            "}\n\n" +
            "var results = [];\n" +
            "for (var i = 0; i < 10; i++) {\n" +
            "    results.push(fibonacci(i));\n" +
            "}\n" +
            "'Fibonacci(0-9): ' + results.join(', ');";
        inputField.setText(js);
        executeCode();
    }

    private void runArrayDemo() {
        String js = "// Array operations\n" +
            "var data = [3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5];\n" +
            "var sorted = data.slice().sort(function(a, b) { return a - b; });\n" +
            "var sum = data.reduce(function(acc, v) { return acc + v; }, 0);\n" +
            "var avg = (sum / data.length).toFixed(2);\n\n" +
            "'Original: ' + data.join(', ') +\n" +
            "'\\nSorted:   ' + sorted.join(', ') +\n" +
            "'\\nSum: ' + sum + ', Avg: ' + avg;";
        inputField.setText(js);
        executeCode();
    }

    private void runObjectDemo() {
        String js = "// Object demo\n" +
            "var player = {\n" +
            "    name: \"LeBron James\",\n" +
            "    team: \"Lakers\",\n" +
            "    stats: { points: 27.1, rebounds: 7.5, assists: 7.4 }\n" +
            "};\n\n" +
            "'Player: ' + player.name +\n" +
            "'\\nTeam: ' + player.team +\n" +
            "'\\nPPG: ' + player.stats.points +\n" +
            "'\\nRPG: ' + player.stats.rebounds +\n" +
            "'\\nAPG: ' + player.stats.assists;";
        inputField.setText(js);
        executeCode();
    }

    // ==================== Java ↔ JS bridge demos ====================

    /**
     * Set Property demo: Create JSObject in Java, set properties, access from JS
     * (mirrors QuickJS Wrapper's Set Property example)
     */
    private void runSetPropertyDemo() {
        appendOutput("=== Set Property Demo ===\n");
        try {
            long start = System.currentTimeMillis();

            JSObject globalObj = hermesContext.getGlobalObject();
            JSObject repository = hermesContext.createNewJSObject();

            // Set typed properties from Java
            repository.setProperty("name", "Hermes Wrapper");
            repository.setProperty("created", 2025);
            repository.setProperty("version", 1.1);
            repository.setProperty("signing_enabled", true);

            // Set a Java callback as JS function
            repository.setProperty("getUrl", (JSCallFunction) args -> {
                return "https://github.com/aspect-build/hermes-wrapper";
            });

            // Attach to global
            globalObj.setProperty("repository", repository);
            repository.release();

            // Now access from JavaScript
            String name = hermesContext.eval("repository.name");
            String created = hermesContext.eval("repository.created");
            String version = hermesContext.eval("repository.version");
            String signing = hermesContext.eval("repository.signing_enabled");
            String url = hermesContext.eval("repository.getUrl()");

            long elapsed = System.currentTimeMillis() - start;

            appendOutput("repository.name = " + name + "\n");
            appendOutput("repository.created = " + created + "\n");
            appendOutput("repository.version = " + version + "\n");
            appendOutput("repository.signing_enabled = " + signing + "\n");
            appendOutput("repository.getUrl() = " + url + "\n");
            appendOutput("(" + elapsed + "ms)\n\n");
        } catch (Exception e) {
            appendOutput("[Error] " + e.getMessage() + "\n\n");
        }
    }

    /**
     * Get Property demo: Define object in JS, read properties from Java
     * (mirrors QuickJS Wrapper's Get Property example)
     */
    private void runGetPropertyDemo() {
        appendOutput("=== Get Property Demo ===\n");
        try {
            long start = System.currentTimeMillis();

            // Define object in JavaScript
            hermesContext.execute(
                "var project = {\n" +
                "  name: 'Hermes Wrapper',\n" +
                "  created: 2025,\n" +
                "  version: 1.1,\n" +
                "  active: true,\n" +
                "  getDesc: function(prefix) { return prefix + ': A Hermes JS engine wrapper for Android'; }\n" +
                "};"
            );

            // Read properties from Java
            JSObject globalObj = hermesContext.getGlobalObject();
            JSObject project = globalObj.getJSObject("project");

            String name = project.getString("name");
            int created = project.getInteger("created");
            double version = project.getDouble("version");
            boolean active = project.getBoolean("active");

            // Call function
            JSFunction getDesc = project.getJSFunction("getDesc");
            Object desc = getDesc.call("Info");
            getDesc.release();
            project.release();

            long elapsed = System.currentTimeMillis() - start;

            appendOutput("project.name = " + name + "\n");
            appendOutput("project.created = " + created + "\n");
            appendOutput("project.version = " + version + "\n");
            appendOutput("project.active = " + active + "\n");
            appendOutput("project.getDesc('Info') = " + desc + "\n");
            appendOutput("(" + elapsed + "ms)\n\n");
        } catch (Exception e) {
            appendOutput("[Error] " + e.getMessage() + "\n\n");
        }
    }

    /**
     * Callback demo: Java function injected into JS, called from JS
     */
    private void runCallbackDemo() {
        appendOutput("=== Java Callback Demo ===\n");
        try {
            long start = System.currentTimeMillis();

            JSObject globalObj = hermesContext.getGlobalObject();

            // Inject a Java function that JS can call
            globalObj.setProperty("javaAdd", (JSCallFunction) args -> {
                if (args.length >= 2) {
                    double a = ((Number) args[0]).doubleValue();
                    double b = ((Number) args[1]).doubleValue();
                    return a + b;
                }
                return 0;
            });

            // Inject a string transform function
            globalObj.setProperty("javaUpperCase", (JSCallFunction) args -> {
                if (args.length >= 1 && args[0] instanceof String) {
                    return ((String) args[0]).toUpperCase();
                }
                return "";
            });

            // Inject a function that returns a function (higher-order)
            globalObj.setProperty("createMultiplier", (JSCallFunction) args -> {
                double factor = args.length > 0 ? ((Number) args[0]).doubleValue() : 1;
                return (JSCallFunction) innerArgs -> {
                    double val = innerArgs.length > 0 ? ((Number) innerArgs[0]).doubleValue() : 0;
                    return val * factor;
                };
            });

            // Call from JavaScript
            String r1 = hermesContext.eval("javaAdd(15, 27)");
            String r2 = hermesContext.eval("javaUpperCase('hello hermes')");
            String r3 = hermesContext.eval("var triple = createMultiplier(3); triple(7)");

            long elapsed = System.currentTimeMillis() - start;

            appendOutput("javaAdd(15, 27) = " + r1 + "\n");
            appendOutput("javaUpperCase('hello hermes') = " + r2 + "\n");
            appendOutput("createMultiplier(3)(7) = " + r3 + "\n");
            appendOutput("(" + elapsed + "ms)\n\n");
        } catch (Exception e) {
            appendOutput("[Error] " + e.getMessage() + "\n\n");
        }
    }

    /**
     * JSArray demo: Create/manipulate arrays from Java
     */
    private void runJSArrayDemo() {
        appendOutput("=== JSArray Demo ===\n");
        try {
            long start = System.currentTimeMillis();

            JSObject globalObj = hermesContext.getGlobalObject();
            JSArray colors = hermesContext.createNewJSArray();

            // Set array elements from Java
            colors.set(0, "red");
            colors.set(1, "green");
            colors.set(2, "blue");
            colors.set(3, "yellow");

            globalObj.setProperty("colors", colors);
            colors.release();

            // Manipulate in JS and read back
            String result = hermesContext.eval(
                "colors.push('purple');\n" +
                "'Array: [' + colors.join(', ') + '], length: ' + colors.length"
            );

            // Read back array from JS
            hermesContext.execute("var nums = [10, 20, 30, 40, 50]");
            JSArray nums = globalObj.getJSArray("nums");
            int len = nums.length();
            StringBuilder sb = new StringBuilder("nums from Java: [");
            for (int i = 0; i < len; i++) {
                if (i > 0) sb.append(", ");
                sb.append(nums.get(i));
            }
            sb.append("]");
            nums.release();

            long elapsed = System.currentTimeMillis() - start;

            appendOutput(result + "\n");
            appendOutput(sb.toString() + "\n");
            appendOutput("(" + elapsed + "ms)\n\n");
        } catch (Exception e) {
            appendOutput("[Error] " + e.getMessage() + "\n\n");
        }
    }

    private void appendOutput(String text) {
        outputView.append(text);
        scrollView.post(() -> scrollView.fullScroll(ScrollView.FOCUS_DOWN));
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (hermesContext != null) {
            hermesContext.close();
        }
    }
}
