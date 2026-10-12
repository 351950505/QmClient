// 请抬头享受阳光｜日子很好 我很我---------致咩子
import assert from 'node:assert/strict';

import { configurationSummary } from './lib/configuration.ts';
import { parseLine, parseLogWithDiagnostics, type ParseResult } from './lib/parse.ts';
import { reportQuality, summarizeForBundle } from './lib/quality.ts';
import { generateReport } from './lib/report.ts';
import { PerfLogCollector } from './lib/stream.ts';

function frameBatchLog(frameCount = 8, durationMs = 1): string {
  return JSON.stringify({
    timestamp: '2026-10-12T12:00:01', system: 'perf/frame', event: 'frame_batch', session: 1,
    frames: Array.from({ length: frameCount }, (_, index) => index + 1),
    durations_ms: Array(frameCount).fill(durationMs),
  });
}

function collectLogs(lines: string[], frameLimit = 16, detailLimit = 3, eventLimit = 2): ParseResult {
  const parsed = parseLogWithDiagnostics(lines.join('\n'));
  assert.equal(parsed.diagnostics.invalidLines, 0);
  const collector = new PerfLogCollector(frameLimit, detailLimit, eventLimit);
  for (const entry of parsed.entries) collector.add(entry);
  return collector.finish(parsed.diagnostics);
}

function assertPublishedSampling(result: ParseResult, verdict: 'PASS' | 'WARN' | 'FAIL', estimated: boolean) {
  const quality = reportQuality(result.entries, result.diagnostics);
  const summary = summarizeForBundle(result.entries, 'sampling.log', result.diagnostics);
  const serialized = JSON.parse(JSON.stringify(summary));
  const html = generateReport(result.entries, 'sampling.log', null, result.diagnostics);

  assert.equal(quality.frameSamplesEstimated, estimated);
  assert.equal(summary.quality.frameSamplesEstimated, estimated);
  assert.equal(serialized.quality.frameSamplesEstimated, estimated);
  assert.equal(summary.verdict, verdict);
  assert.equal(serialized.verdict, verdict);
  assert.match(html, new RegExp(`class="verdict-banner [^"]+">${verdict}<`));
  const samplingWarning = quality.warnings.find(warning => warning.startsWith('analysis_sampled:'));
  assert.ok(samplingWarning);
  if (estimated) {
    assert.match(samplingWarning, /百分位和合规率为估计值/);
    assert.match(html, /analysis_sampled:[^<]*百分位和合规率为估计值/);
  } else {
    assert.match(samplingWarning, /归因/);
    assert.doesNotMatch(samplingWarning, /百分位和合规率为估计值/);
    assert.doesNotMatch(html, /analysis_sampled:[^<]*百分位和合规率为估计值/);
  }
  return { quality, summary, serialized, html };
}

function testDetailSamplingKeepsCompleteUnifiedFrameVerdict() {
  const result = collectLogs([
    frameBatchLog(),
    ...Array.from({ length: 12 }, (_, index) =>
      `2026-10-12 12:00:01 I perf/gameclient: stage=components_total duration_ms=1000 frame=${index + 1}`),
  ]);

  assert.equal(result.diagnostics.sampledEntries, 9);
  assert.equal(result.diagnostics.sampledFrameEntries, 0);
  assert.equal(result.diagnostics.sampledDetailEntries, 9);
  assert.equal(result.diagnostics.droppedEventEntries, 0);
  const { quality, summary, serialized } = assertPublishedSampling(result, 'PASS', false);
  assert.equal(quality.sampleCount, 8);
  assert.equal(quality.sampledFrameEntries, 0);
  assert.equal(quality.sampledDetailEntries, 9);
  assert.equal(quality.droppedEventEntries, 0);
  assert.equal(summary.percentiles.p99, 1);
  assert.equal(serialized.quality.sampledDetailEntries, 9);
  assert.match(quality.warnings.join('\n'), /明细[^\n]*归因/);
}

function testWindowTruncationKeepsCompleteUnifiedFrameVerdict() {
  const result = collectLogs([
    frameBatchLog(),
    ...Array.from({ length: 7 }, (_, index) =>
      `2026-10-12 12:00:01 I perf/session: event=checkpoint frame=${index + 1} session=1`),
  ]);

  assert.equal(result.diagnostics.sampledEntries, 5);
  assert.equal(result.diagnostics.sampledFrameEntries, 0);
  assert.equal(result.diagnostics.sampledDetailEntries, 0);
  assert.equal(result.diagnostics.droppedEventEntries, 5);
  const { quality, serialized } = assertPublishedSampling(result, 'PASS', false);
  assert.equal(quality.sampleCount, 8);
  assert.equal(quality.droppedEventEntries, 5);
  assert.equal(serialized.quality.droppedEventEntries, 5);
  assert.match(quality.warnings.join('\n'), /窗口[^\n]*归因/);
}

function testUnifiedFrameSamplingMarksFrameStatisticsEstimated() {
  const result = collectLogs([frameBatchLog()], 3);

  assert.equal(result.diagnostics.sampledEntries, 5);
  assert.equal(result.diagnostics.sampledFrameEntries, 5);
  assert.equal(result.diagnostics.sampledDetailEntries, 0);
  assert.equal(result.diagnostics.droppedEventEntries, 0);
  const { quality, serialized } = assertPublishedSampling(result, 'WARN', true);
  assert.equal(quality.sampleCount, 3);
  assert.equal(quality.sampledFrameEntries, 5);
  assert.equal(serialized.quality.sampledFrameEntries, 5);
}

function testUnifiedFrameSamplingPreservesWarningAndFailureVerdicts() {
  for (const { durationMs, verdict } of [
    { durationMs: 20, verdict: 'WARN' },
    { durationMs: 40, verdict: 'FAIL' },
  ] as const) {
    const lines = [frameBatchLog(4, durationMs)];
    const complete = collectLogs(lines);
    const completeSummary = summarizeForBundle(complete.entries, 'complete.log', complete.diagnostics);
    assert.equal(completeSummary.verdict, verdict, `${durationMs}ms complete frames`);
    assert.equal(completeSummary.quality.frameSamplesEstimated, false);

    const sampled = collectLogs(lines, 3);
    assert.equal(sampled.diagnostics.sampledFrameEntries, 1, `${durationMs}ms sampled frames`);
    const { quality, summary } = assertPublishedSampling(sampled, verdict, true);
    assert.equal(quality.sampleCount, 3);
    assert.equal(summary.percentiles.p99, durationMs);
  }
}

function testLegacyFrameSamplingUsesDetailPoolLoss() {
  for (const system of ['perf/menu', 'perf/gameclient']) {
    const result = collectLogs(Array.from({ length: 8 }, (_, index) =>
      `2026-10-12 12:00:01 I ${system}: stage=render_total duration_ms=1 frame=${index + 1}`));

    assert.equal(result.diagnostics.sampledFrameEntries, 0, system);
    assert.equal(result.diagnostics.sampledDetailEntries, 5, system);
    const { quality, serialized } = assertPublishedSampling(result, 'WARN', true);
    assert.equal(quality.sampleCount, 3, system);
    assert.equal(quality.sampledFrameEntries, 0, system);
    assert.equal(quality.sampledDetailEntries, 5, system);
    assert.equal(serialized.quality.sampledDetailEntries, 5, system);
  }
}

function testWindowTruncationKeepsCompleteLegacyFrameVerdict() {
  for (const system of ['perf/menu', 'perf/gameclient']) {
    const result = collectLogs([
      ...Array.from({ length: 8 }, (_, index) =>
        `2026-10-12 12:00:01 I ${system}: stage=render_total duration_ms=1 frame=${index + 1}`),
      ...Array.from({ length: 7 }, (_, index) =>
        `2026-10-12 12:00:01 I perf/session: event=checkpoint frame=${index + 1} session=1`),
    ], 16, 16);

    assert.equal(result.diagnostics.sampledEntries, 5, system);
    assert.equal(result.diagnostics.sampledFrameEntries, 0, system);
    assert.equal(result.diagnostics.sampledDetailEntries, 0, system);
    assert.equal(result.diagnostics.droppedEventEntries, 5, system);
    const { quality, summary, serialized } = assertPublishedSampling(result, 'PASS', false);
    assert.equal(quality.sampleCount, 8, system);
    assert.equal(summary.percentiles.p99, 1, system);
    assert.equal(serialized.quality.droppedEventEntries, 5, system);
    assert.match(quality.warnings.join('\n'), /窗口[^\n]*归因/);
  }
}

function testLegacyDiagnosticsConservativelyEstimateFrameStatistics() {
  const parsed = parseLogWithDiagnostics(frameBatchLog());
  const result = {
    entries: parsed.entries,
    diagnostics: { totalLines: 10, invalidLines: 0, sampledEntries: 2 },
  };

  const { quality, serialized } = assertPublishedSampling(result, 'WARN', true);
  assert.equal(quality.sampledFrameEntries, null);
  assert.equal(quality.sampledDetailEntries, null);
  assert.equal(quality.droppedEventEntries, null);
  assert.equal(serialized.quality.sampledFrameEntries, null);
  assert.equal(serialized.quality.sampledDetailEntries, null);
  assert.equal(serialized.quality.droppedEventEntries, null);
  assert.match(quality.warnings.join('\n'), /旧诊断[^\n]*归因/);
}

function testLegacyFrameSourceWithMissingDetailCountUsesTotalLoss() {
  const parsed = parseLogWithDiagnostics('2026-10-12 12:00:01 I perf/menu: stage=render_total duration_ms=1 frame=1');
  const result = {
    entries: parsed.entries,
    diagnostics: { totalLines: 3, invalidLines: 0, sampledEntries: 2, sampledFrameEntries: 0 },
  };

  const { quality } = assertPublishedSampling(result, 'WARN', true);
  assert.equal(quality.sampledFrameEntries, 0);
  assert.equal(quality.sampledDetailEntries, null);
}

function testUnifiedCollectorBoundsMemoryAndKeepsLatestConfig() {
  const collector = new PerfLogCollector(16, 8, 8);
  for (let i = 0; i < 1000; i++) {
    collector.add(parseLine(JSON.stringify({ system: 'perf/frame', event: 'frame_sample', duration_ms: i % 20 + 1, frame: i }))!);
    collector.add(parseLine(JSON.stringify({ system: 'perf/config', event: 'config_value', session: 1, name: 'tc_enabled', owner: 'tclient', type: 'int', value: String(i), initial: i === 0 ? 1 : 0, revision: i + 1, part: 0, parts: 1 }))!);
  }
  const result = collector.finish({ totalLines: 2000, invalidLines: 0 });
  assert.ok(result.entries.length <= 18);
  assert.equal(result.diagnostics.sampledEntries, 984);
  assert.equal(result.diagnostics.sampledFrameEntries, 984);
  assert.equal(result.diagnostics.sampledDetailEntries, 0);
  assert.equal(result.diagnostics.droppedEventEntries, 0);
  const config = configurationSummary(result.entries).variables[0];
  assert.equal(config.initialValue, 0);
  assert.equal(config.value, 999);
  assert.ok(reportQuality(result.entries, result.diagnostics).warnings.some(warning => warning.startsWith('analysis_sampled:')));
}

export function runSamplingTests() {
  testDetailSamplingKeepsCompleteUnifiedFrameVerdict();
  testWindowTruncationKeepsCompleteUnifiedFrameVerdict();
  testUnifiedFrameSamplingMarksFrameStatisticsEstimated();
  testUnifiedFrameSamplingPreservesWarningAndFailureVerdicts();
  testLegacyFrameSamplingUsesDetailPoolLoss();
  testWindowTruncationKeepsCompleteLegacyFrameVerdict();
  testLegacyDiagnosticsConservativelyEstimateFrameStatistics();
  testLegacyFrameSourceWithMissingDetailCountUsesTotalLoss();
  testUnifiedCollectorBoundsMemoryAndKeepsLatestConfig();
}
