#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

// C++から渡されるテクスチャデータ
uniform sampler2D screenTexture;

// C++から渡されるトーンマッピング用のパラメータ
uniform float u_minVal; // 最小輝度 (0.0 - 1.0)
uniform float u_maxVal; // 最大輝度 (0.0 - 1.0)
uniform int u_channels; // 画像のチャンネル数 (1:Gray, 3:RGB, 4:RGBA)

void main()
{
    // テクスチャから色を取得 (値は0.0 - 1.0の範囲に正規化されている)
    vec4 texColor = texture(screenTexture, TexCoords);
    
    // チャンネル数に応じて処理を分岐
    vec3 color;
    if (u_channels == 1) {
        // グレースケール画像の場合、赤(r)成分のみ使用
        float value = texColor.r;
        float scaled_value = (value - u_minVal) / (u_maxVal - u_minVal);
        color = vec3(clamp(scaled_value, 0.0, 1.0));
    } else {
        // カラー画像の場合、RGB各成分に適用
        vec3 scaled_value = (texColor.rgb - vec3(u_minVal)) / (u_maxVal - u_minVal);
        color = clamp(scaled_value, 0.0, 1.0);
    }
    
    // 最終的な色として出力 (アルファは1.0で不透明)
    FragColor = vec4(color, 1.0);
}
