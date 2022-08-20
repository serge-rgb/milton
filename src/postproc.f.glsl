// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

uniform sampler2D u_canvas;
uniform vec2 u_screen_size;

vec4 FxaaPixelShader(
    vec2 pos,
    vec4 fxaaConsolePosPos,
    sampler2D tex,
    sampler2D fxaaConsole360TexExpBiasNegOne,
    sampler2D fxaaConsole360TexExpBiasNegTwo,
    vec2 fxaaQualityRcpFrame,
    vec4 fxaaConsoleRcpFrameOpt,
    vec4 fxaaConsoleRcpFrameOpt2,
    vec4 fxaaConsole360RcpFrameOpt2,
    float fxaaQualitySubpix,
    float fxaaQualityEdgeThreshold,
    float fxaaQualityEdgeThresholdMin,
    float fxaaConsoleEdgeSharpness,
    float fxaaConsoleEdgeThreshold,
    float fxaaConsoleEdgeThresholdMin,
    vec4 fxaaConsole360ConstDir
);

void
main()
{
    // Pixel width / height.
    float pw = 1 / u_screen_size.x;
    float ph = 1 / u_screen_size.y;
    vec2 coord = gl_FragCoord.xy / u_screen_size;

    out_color = texture(u_canvas, coord, 0);
    out_color.rgb = FxaaPixelShader(
        // Use noperspective interpolation here (turn off perspective interpolation).
        // {xy} = center of pixel
        gl_FragCoord.xy / u_screen_size, // FxaaFloat2 pos,
        //
        // Used only for FXAA Console, and not used on the 360 version.
        // Use noperspective interpolation here (turn off perspective interpolation).
        // {xy__} = upper left of pixel
        // {__zw} = lower right of pixel
        vec4(0), // FxaaFloat4 fxaaConsolePosPos,
        //
        // Input color texture.
        // {rgb_} = color in linear or perceptual color space
        // if (FXAA_GREEN_AS_LUMA == 0)
        //     {___a} = luma in perceptual color space (not linear)
        u_canvas, // FxaaTex tex,
        //
        // Only used on the optimized 360 version of FXAA Console.
        // For everything but 360, just use the same input here as for tex.
        // For 360, same texture, just alias with a 2nd sampler.
        // This sampler needs to have an exponent bias of -1.
        u_canvas,  // FxaaTex fxaaConsole360TexExpBiasNegOne,
        //
        // Only used on the optimized 360 version of FXAA Console.
        // For everything but 360, just use the same input here as for texÏ.
        // For 360, same texture, just alias with a 3rd sampler.
        // This sampler needs to have an exponent bias of -2.
        u_canvas, // FxaaTex fxaaConsole360TexExpBiasNegTwo,
        //
        // Only used on FXAA Quality.
        // This must be from a constant/uniform.
        // {x_} = 1.0/screenWidthInPixels
        // {_y} = 1.0/screenHeightInPixels
        1/u_screen_size, //FxaaFloat2 fxaaQualityRcpFrame,
        //
        // Only used on FXAA Console.
        // This must be from a constant/uniform.
        // This effects sub-pixel AA quality and inversely sharpness.
        //   Where N ranges between,
        //     N = 0.50 (default)
        //     N = 0.33 (sharper)
        // {x___} = -N/screenWidthInPixels
        // {_y__} = -N/screenHeightInPixels
        // {__z_} =  N/screenWidthInPixels
        // {___w} =  N/screenHeightInPixels
        vec4(0), // FxaaFloat4 fxaaConsoleRcpFrameOpt,
        //
        // Only used on FXAA Console.
        // Not used on 360, but used on PS3 and PC.
        // This must be from a constant/uniform.
        // {x___} = -2.0/screenWidthInPixels
        // {_y__} = -2.0/screenHeightInPixels
        // {__z_} =  2.0/screenWidthInPixels
        // {___w} =  2.0/screenHeightInPixels
        vec4(0), //FxaaFloat4 fxaaConsoleRcpFrameOpt2,
        //
        // Only used on FXAA Console.
        // Only used on 360 in place of fxaaConsoleRcpFrameOpt2.
        // This must be from a constant/uniform.
        // {x___} =  8.0/screenWidthInPixels
        // {_y__} =  8.0/screenHeightInPixels
        // {__z_} = -4.0/screenWidthInPixels
        // {___w} = -4.0/screenHeightInPixels
        vec4(0), // FxaaFloat4 fxaaConsole360RcpFrameOpt2,
        //
        // Only used on FXAA Quality.
        // This used to be the FXAA_QUALITY__SUBPIX define.
        // It is here now to allow easier tuning.
        // Choose the amount of sub-pixel aliasing removal.
        // This can effect sharpness.
        //   1.00 - upper limit (softer)
        //   0.75 - default amount of filtering
        //   0.50 - lower limit (sharper, less sub-pixel aliasing removal)
        //   0.25 - almost off
        //   0.00 - completely off
        0.25, // FxaaFloat fxaaQualitySubpix,
        //
        // Only used on FXAA Quality.
        // This used to be the FXAA_QUALITY__EDGE_THRESHOLD define.
        // It is here now to allow easier tuning.
        // The minimum amount of local contrast required to apply algorithm.
        //   0.333 - too little (faster)
        //   0.250 - low quality
        //   0.166 - default
        //   0.125 - high quality
        //   0.063 - overkill (slower)
        0.063, // FxaaFloat fxaaQualityEdgeThreshold,
        //
        // Only used on FXAA Quality.
        // This used to be the FXAA_QUALITY__EDGE_THRESHOLD_MIN define.
        // It is here now to allow easier tuning.
        // Trims the algorithm from processing darks.
        //   0.0833 - upper limit (default, the start of visible unfiltered edges)
        //   0.0625 - high quality (faster)
        //   0.0312 - visible limit (slower)
        // Special notes when using FXAA_GREEN_AS_LUMA,
        //   Likely want to set this to zero.
        //   As colors that are mostly not-green
        //   will appear very dark in the green channel!
        //   Tune by looking at mostly non-green content,
        //   then start at zero and increase until aliasing is a problem.
        0.0312, // FxaaFloat fxaaQualityEdgeThresholdMin,
        //
        // Only used on FXAA Console.
        // This used to be the FXAA_CONSOLE__EDGE_SHARPNESS define.
        // It is here now to allow easier tuning.
        // This does not effect PS3, as this needs to be compiled in.
        //   Use FXAA_CONSOLE__PS3_EDGE_SHARPNESS for PS3.
        //   Due to the PS3 being ALU bound,
        //   there are only three safe values here: 2 and 4 and 8.
        //   These options use the shaders ability to a free *|/ by 2|4|8.
        // For all other platforms can be a non-power of two.
        //   8.0 is sharper (default!!!)
        //   4.0 is softer
        //   2.0 is really soft (good only for vector graphics inputs)
        0.0, // FxaaFloat fxaaConsoleEdgeSharpness,
        //
        // Only used on FXAA Console.
        // This used to be the FXAA_CONSOLE__EDGE_THRESHOLD define.
        // It is here now to allow easier tuning.
        // This does not effect PS3, as this needs to be compiled in.
        //   Use FXAA_CONSOLE__PS3_EDGE_THRESHOLD for PS3.
        //   Due to the PS3 being ALU bound,
        //   there are only two safe values here: 1/4 and 1/8.
        //   These options use the shaders ability to a free *|/ by 2|4|8.
        // The console setting has a different mapping than the quality setting.
        // Other platforms can use other values.
        //   0.125 leaves less aliasing, but is softer (default!!!)
        //   0.25 leaves more aliasing, and is sharper
        0.0, //FxaaFloat fxaaConsoleEdgeThreshold,
        //
        // Only used on FXAA Console.
        // This used to be the FXAA_CONSOLE__EDGE_THRESHOLD_MIN define.
        // It is here now to allow easier tuning.
        // Trims the algorithm from processing darks.
        // The console setting has a different mapping than the quality setting.
        // This only applies when FXAA_EARLY_EXIT is 1.
        // This does not apply to PS3,
        // PS3 was simplified to avoid more shader instructions.
        //   0.06 - faster but more aliasing in darks
        //   0.05 - default
        //   0.04 - slower and less aliasing in darks
        // Special notes when using FXAA_GREEN_AS_LUMA,
        //   Likely want to set this to zero.
        //   As colors that are mostly not-green
        //   will appear very dark in the green channel!
        //   Tune by looking at mostly non-green content,
        //   then start at zero and increase until aliasing is a problem.
        0.0, // FxaaFloat fxaaConsoleEdgeThresholdMin,
        //
        // Extra constants for 360 FXAA Console only.
        // Use zeros or anything else for other platforms.
        // These must be in physical constant registers and NOT immedates.
        // Immedates will result in compiler un-optimizing.
        // {xyzw} = float4(1.0, -1.0, 0.25, -0.25)
        vec4(0) // FxaaFloat4 fxaaConsole360ConstDir
    ).rgb;
}
