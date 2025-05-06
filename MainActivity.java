package com.example.autko;

import android.os.Bundle;
import android.view.View;
import android.view.MotionEvent;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;
import org.java_websocket.client.WebSocketClient;
import org.java_websocket.handshake.ServerHandshake;
import org.json.JSONException;
import org.json.JSONObject;
import java.net.URI;
import java.net.URISyntaxException;
import android.util.Log;

public class MainActivity extends AppCompatActivity {

    private WebSocketClient mWebSocketClient;
    private String serverURI = "ws://192.168.4.1:81";  // Adres IP ESP32 w trybie AP i port WebSocket
    private TextView tempTextView;
    private int frontDistance = 0;
    private int backDistance = 0;
    private boolean isMoving = false; // Flaga, aby śledzić, czy pojazd jest w ruchu
    private String currentDirection = ""; // Kierunek ruchu (FORWARD, BACKWARD)

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        connectWebSocket();

        Button forwardButton = findViewById(R.id.forwardButton);
        Button backwardButton = findViewById(R.id.backwardButton);
        Button leftButton = findViewById(R.id.leftButton);
        Button rightButton = findViewById(R.id.rightButton);

        Button btnToggleLights = findViewById(R.id.btnToggleLights);
        btnToggleLights.setOnClickListener(v -> {
            if (mWebSocketClient != null && mWebSocketClient.isOpen()) {
                mWebSocketClient.send("TOGGLE_LIGHTS");
            }
        });

        setupButtonControl(forwardButton, "FORWARD");
        setupButtonControl(backwardButton, "BACKWARD");
        setupButtonControl(leftButton, "LEFT");
        setupButtonControl(rightButton, "RIGHT");

        // Ustawienia TextView do wyświetlania odległości
        TextView frontDistanceTextView = findViewById(R.id.frontDistanceTextView);
        TextView backDistanceTextView = findViewById(R.id.backDistanceTextView);
        tempTextView = findViewById(R.id.tempTextView);
    }

    private void setupButtonControl(Button button, String command) {
        button.setOnTouchListener((v, event) -> {
            if (event.getAction() == MotionEvent.ACTION_DOWN) {
                // Sprawdzenie, czy można wysłać komendę w danym kierunku
                if (command.equals("FORWARD") && frontDistance < 25 && frontDistance > 0) {
                    Toast.makeText(this, "Przeszkoda z przodu!", Toast.LENGTH_SHORT).show();
                    sendMessage("STOP");
                    isMoving = false;
                    return true; // Zatrzymanie, jeśli zbyt blisko
                }
                if (command.equals("BACKWARD") && backDistance < 25 && backDistance > 0) {
                    Toast.makeText(this, "Przeszkoda z tyłu!", Toast.LENGTH_SHORT).show();
                    sendMessage("STOP");
                    isMoving = false;
                    return true; // Zatrzymanie, jeśli zbyt blisko
                }

                // Ustawienie kierunku na podstawie komendy
                currentDirection = command;

                if (command.equals("LEFT") || command.equals("RIGHT")) {
                    sendMessage(command);
                } else {
                    if (!isMoving) {
                        sendMessage(command);
                        isMoving = true;
                    }
                }
            } else if (event.getAction() == MotionEvent.ACTION_UP) {
                if (command.equals("LEFT") || command.equals("RIGHT")) {
                    sendMessage(command + "_RELEASE");
                } else {
                    sendMessage("STOP");
                    isMoving = false;
                }
            }
            return true;
        });
    }

    // Funkcja do łączenia się z WebSocket
    private void connectWebSocket() {
        try {
            URI uri = new URI(serverURI);
            mWebSocketClient = new WebSocketClient(uri) {
                @Override
                public void onOpen(ServerHandshake handshakedata) {
                    runOnUiThread(() -> Toast.makeText(MainActivity.this, "Połączono z ESP32", Toast.LENGTH_SHORT).show());
                }

                @Override
                public void onMessage(String message) {
                    // Obsługuje odebrane dane o odległości
                    runOnUiThread(() -> {
                        try {
                            JSONObject json = new JSONObject(message);
                            int front = json.getInt("front");
                            int back = json.getInt("back");

                            // Aktualizacja stanu przeszkód
                            updateObstacleState(front, back);
                            // wyswietlanie temperatury
                            if (json.has("temperature")) {
                                float temp = (float) json.getDouble("temperature");
                                updateTemperature(temp);
                            }

                            // Wyświetlanie odległości na ekranie
                            Log.d("WebSocket", "Dystans przód: " + front + ", Dystans tył: " + back);
                        } catch (JSONException e) {
                            e.printStackTrace();
                        }
                    });
                }

                @Override
                public void onClose(int code, String reason, boolean remote) {
                    runOnUiThread(() -> Toast.makeText(MainActivity.this, "Rozłączono", Toast.LENGTH_SHORT).show());
                }

                @Override
                public void onError(Exception ex) {
                    ex.printStackTrace();
                }
            };
            mWebSocketClient.connect();
        } catch (URISyntaxException e) {
            e.printStackTrace();
        }
    }

    // Funkcja do wysyłania komend do ESP32
    private void sendMessage(String message) {
        if (mWebSocketClient != null && mWebSocketClient.isOpen()) {
            mWebSocketClient.send(message);
        } else {
            Toast.makeText(MainActivity.this, "Brak połączenia z ESP32", Toast.LENGTH_SHORT).show();
        }
    }

    private void updateTemperature(float temp) {
        if (tempTextView != null) {
            tempTextView.setText("Temperatura: " + temp + " °C");
        }
    }

    // Funkcja do aktualizacji stanu przeszkód
    private void updateObstacleState(int front, int back) {
        frontDistance = front;
        backDistance = back;

        // Sprawdzanie, czy przeszkoda występuje z przodu, tylko jeśli jedziemy do przodu
        if (currentDirection.equals("FORWARD") && frontDistance < 25 && frontDistance > 0 ) {
            Toast.makeText(this, "Przeszkoda z przodu!", Toast.LENGTH_SHORT).show();
            if (isMoving) {
                sendMessage("STOP");
                isMoving = false;
            }
        }

        // Sprawdzanie, czy przeszkoda występuje z tyłu, tylko jeśli jedziemy do tyłu
        if (currentDirection.equals("BACKWARD") && backDistance < 25 && backDistance > 0) {
            Toast.makeText(this, "Przeszkoda z tyłu!", Toast.LENGTH_SHORT).show();
            if (isMoving) {
                sendMessage("STOP");
                isMoving = false;
            }
        }

        // Aktualizacja wyświetlanych odległości
        TextView frontDistanceTextView = findViewById(R.id.frontDistanceTextView);
        TextView backDistanceTextView = findViewById(R.id.backDistanceTextView);

        frontDistanceTextView.setText("Dystans przód: " + frontDistance + " cm");
        backDistanceTextView.setText("Dystans tył: " + backDistance + " cm");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mWebSocketClient != null) {
            mWebSocketClient.close();
        }
    }
}
