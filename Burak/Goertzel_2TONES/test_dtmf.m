%% DTMF decoding test
clear; clc; close all;
rng(7);

Fs = 16000;
N = 800;
frow = [697 770 852 941];
fcol = [1209 1336 1477 1633];
frequencies = [frow fcol];
keys = ['123A';'456B';'789C';'*0#D'];
reference = '123A456B789C*0#D';
frequency_labels = arrayfun(@num2str,frequencies,'UniformOutput',false);

% Generate simulated ADC samples with a 2048-count DC offset.
frames_per_tone = 1;
frames_per_gap = 2;
full_signal = zeros(0,1,'uint16');
expected_block = '';
for idx = 1:numel(reference)
    [row,col] = find(keys == reference(idx));
    amplitude = 250 + 30*mod(idx,5);
    t = (0:frames_per_tone*N-1)'/Fs;
    tone = (sin(2*pi*frow(row)*t) + sin(2*pi*fcol(col)*t))/2;
    adc_tone = uint16(round(min(4095,max(0,2048+amplitude*tone+4*randn(size(t))))));
    adc_gap = uint16(round(min(4095,max(0,2048+4*randn(frames_per_gap*N,1)))));
    full_signal = [full_signal; adc_tone; adc_gap]; %#ok<AGROW>
    expected_block = [expected_block repmat(reference(idx),1,frames_per_tone) ...
        repmat('-',1,frames_per_gap)]; %#ok<AGROW>
end

% Decode each 800-sample block.
blocks = numel(full_signal)/N;
magnitudes = zeros(blocks,8);
detected = repmat('-',1,blocks);
for i = 1:blocks
    sample_block = full_signal((i-1)*N+(1:N));
    [key, mag] = dtmf_decode(sample_block);
    magnitudes(i,:) = double(mag);
    if key ~= 0
        detected(i) = char(key);
    end
end

fprintf('Input key sequence:       %s\n',reference);
fprintf('Expected block sequence:  %s\n',expected_block);
fprintf('Decoded block sequence:   %s\n',detected);
passes = sum(expected_block == detected);
fprintf('Correct blocks: %d / %d (%.1f%%)\n',passes,blocks,100*passes/blocks);
assert(passes == blocks,'Not all simulated blocks decoded correctly.');

%% Input signal spectrogram
NFFT = 128;
overlap = 115;
hop = NFFT-overlap;
winfft = 0.54-0.46*cos(2*pi*(0:NFFT-1)'/(NFFT-1));
starts = 1:hop:(numel(full_signal)-NFFT+1);
P = zeros(NFFT/2+1,numel(starts));
for i = 1:numel(starts)
    s = double(full_signal(starts(i):starts(i)+NFFT-1));
    s = s-mean(s);
    F = fft(s.*winfft,NFFT);
    P(:,i) = 20*log10(abs(F(1:NFFT/2+1))+1e-6);
end
figure('Name','Input Spectrogram','Color','w','Position',[100 100 850 500]);
imagesc((0:NFFT/2)*Fs/NFFT/1000,(starts-1)/Fs*1000,P');
axis xy;
colormap(parula);
colorbar;
xlabel('Frequency (kHz)');
ylabel('Time (ms)');
title('DTMF Input Spectrogram');
set(gca,'FontSize',11,'LineWidth',1,'Box','on');
saveas(gcf,'spectrogram.png');

%% Goertzel magnitudes
block_ms = 1000*N/Fs;
time_edges = (0:blocks)*block_ms;
figure('Name','Goertzel Magnitudes','Color','w','Position',[130 120 850 520]);
imagesc(1:8,[(0.5)*block_ms (blocks-0.5)*block_ms],magnitudes);
set(gca,'YDir','normal','FontSize',11,'LineWidth',1,'Box','on');
xlim([0.5 8.5]);
ylim([time_edges(1) time_edges(end)]);
xticks(1:8);
xticklabels(frequency_labels);
xlabel('DTMF Frequency (Hz)');
ylabel('Time (ms)');
title(sprintf('Goertzel Magnitudes (%d Samples/Block)',N));
colormap(parula);
cb = colorbar;
cb.Label.String = 'Goertzel magnitude';
hold on;
xline(4.5,'w--','LineWidth',1.5);
hold off;
saveas(gcf,'goertzel_magnitudes.png');

%% Detected frequencies
above = false(size(magnitudes));
for i = 1:blocks
    if std(double(full_signal((i-1)*N+(1:N))),1) >= 10
        above(i,:) = magnitudes(i,:) > 0.45*max(magnitudes(i,:));
    end
end
figure('Name','Detected DTMF Frequencies','Color','w','Position',[160 140 850 520]);
imagesc(1:8,[(0.5)*block_ms (blocks-0.5)*block_ms],double(above));
set(gca,'YDir','normal','FontSize',11,'LineWidth',1,'Box','on');
xlim([0.5 8.5]);
ylim([time_edges(1) time_edges(end)]);
xticks(1:8);
xticklabels(frequency_labels);
xlabel('DTMF Frequency (Hz)');
ylabel('Time (ms)');
title('Detected DTMF Frequencies');
colormap([0.14 0.13 0.48; 0.98 0.86 0.16]);
caxis([0 1]);
cb = colorbar;
cb.Ticks = [0 1];
cb.TickLabels = {'Not detected','Detected'};
hold on;
xline(4.5,'w--','LineWidth',1.5);
hold off;
saveas(gcf,'detected_frequencies.png');

%% Check silence and noise
no_signal = uint16(2048*ones(N,1));
[k0,~] = dtmf_decode(no_signal);
assert(k0 == 0,'DC-only input must not be decoded as a key');
noisy_silence = uint16(round(2048+3*randn(N,1)));
[kn,~] = dtmf_decode(noisy_silence);
assert(kn == 0,'Quiet noisy input must not be decoded as a key');
fprintf('Silence and quiet-noise rejection: PASS\n');

writematrix(full_signal,'adc_test_samples.csv');
fprintf('Created three figures and adc_test_samples.csv\n');
