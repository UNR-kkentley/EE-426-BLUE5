function [key, magnitudes] = dtmf_decode(adc_samples)
% Decode one 800-sample ADC block sampled at 16 kHz.
%#codegen

    N = 800;
    magnitudes = zeros(1, 8, 'single');
    key = uint8(0);

    Fs = 16000;
    frequencies = single([697 770 852 941 1209 1336 1477 1633]);
    keys = ['123A'; '456B'; '789C'; '*0#D'];

    mean_adc = single(0);
    for n = 1:N
        mean_adc = mean_adc + single(adc_samples(n));
    end
    mean_adc = mean_adc / single(N);

    % Ignore blocks with very little signal.
    energy = single(0);
    for n = 1:N
        v = single(adc_samples(n)) - mean_adc;
        energy = energy + v*v;
    end
    rms_adc = sqrt(energy / single(N));
    if rms_adc < single(10)
        return;
    end

    for j = 1:8
        omega = single(2*pi) * frequencies(j) / single(Fs);
        cw = cos(omega);
        sw = sin(omega);
        coeff = single(2)*cw;
        z1 = single(0);
        z2 = single(0);
        for n = 1:N
            hamming_value = single(0.54) - single(0.46)* ...
                cos(single(2*pi*(n-1)/(N-1)));
            x = (single(adc_samples(n)) - mean_adc)*hamming_value;
            z0 = x + coeff*z1 - z2;
            z2 = z1;
            z1 = z0;
        end
        I = cw*z1 - z2;
        Q = sw*z1;
        magnitudes(j) = sqrt(I*I + Q*Q);
    end

    % Select one frequency from each DTMF group.
    peak = max(magnitudes);
    threshold = single(0.45)*peak;
    row_count = 0;
    col_count = 0;
    row_index = 0;
    col_index = 0;

    for j = 1:4
        if magnitudes(j) > threshold
            row_count = row_count + 1;
            row_index = j;
        end
        if magnitudes(j+4) > threshold
            col_count = col_count + 1;
            col_index = j;
        end
    end
    if row_count == 1 && col_count == 1
        key = uint8(keys(row_index, col_index));
    end
end
