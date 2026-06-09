package com.hermes.wrapper.demo;

import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.TextView;
import android.appcompat.app.AppCompatActivity;

import com.hermes.wrapper.HermesContext;
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

        // Demo buttons
        findViewById(R.id.btn_demo_basic).setOnClickListener(v -> runBasicDemo());
        findViewById(R.id.btn_demo_function).setOnClickListener(v -> runFunctionDemo());
        findViewById(R.id.btn_demo_array).setOnClickListener(v -> runArrayDemo());
        findViewById(R.id.btn_demo_object).setOnClickListener(v -> runObjectDemo());

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
            appendOutput("> " + code + "\n");
            appendOutput(result + " (" + elapsed + "ms)\n\n");
        } catch (Exception e) {
            appendOutput("[Error] " + e.getMessage() + "\n\n");
        }
    }

    private void runBasicDemo() {
        String js = """
            // Basic calculations
            var a = 10;
            var b = 20;
            var sum = a + b;
            var product = a * b;
            'Sum: ' + sum + ', Product: ' + product;
            """;
        inputField.setText(js);
        appendOutput("// Basic Demo loaded. Tap Run to execute.\n");
    }

    private void runFunctionDemo() {
        String js = """
            // Function demo
            function fibonacci(n) {
                if (n <= 1) return n;
                return fibonacci(n - 1) + fibonacci(n - 2);
            }
            
            var results = [];
            for (var i = 0; i < 10; i++) {
                results.push(fibonacci(i));
            }
            'Fibonacci(0-9): ' + results.join(', ');
            """;
        inputField.setText(js);
        appendOutput("// Function Demo loaded. Tap Run to execute.\n");
    }

    private void runArrayDemo() {
        String js = """
            // Array operations
            var data = [3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5];
            var sorted = data.slice().sort(function(a, b) { return a - b; });
            var sum = data.reduce(function(acc, v) { return acc + v; }, 0);
            var avg = (sum / data.length).toFixed(2);
            
            'Original: ' + data.join(', ') +
            '\nSorted:   ' + sorted.join(', ') +
            '\nSum: ' + sum + ', Avg: ' + avg;
            """;
        inputField.setText(js);
        appendOutput("// Array Demo loaded. Tap Run to execute.\n");
    }

    private void runObjectDemo() {
        String js = """
            // Object demo
            var player = {
                name: "LeBron James",
                team: "Lakers",
                stats: { points: 27.1, rebounds: 7.5, assists: 7.4 }
            };
            
            'Player: ' + player.name +
            '\nTeam: ' + player.team +
            '\nPPG: ' + player.stats.points +
            '\nRPG: ' + player.stats.rebounds +
            '\nAPG: ' + player.stats.assists;
            """;
        inputField.setText(js);
        appendOutput("// Object Demo loaded. Tap Run to execute.\n");
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
