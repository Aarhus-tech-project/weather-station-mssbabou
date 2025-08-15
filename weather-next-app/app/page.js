'use client';

import { useEffect, useRef, useState } from 'react';
import { RotateCcw, Square, CloudSun, Plus, Trash2 } from 'lucide-react';
import { Thermometer, Droplets, Gauge, Wind, CloudFog } from 'lucide-react';

export default function Home() {
  const [output, setOutput] = useState('');
  const [isStreaming, setIsStreaming] = useState(false);
  const abortRef = useRef(null);
  const boxRef = useRef(null);

  // sidebar state
  const [tempInput, setTempInput] = useState('');
  const [rows, setRows] = useState([]);

  async function run() {
    if (isStreaming) return;
    setOutput('');
    setIsStreaming(true);

    const controller = new AbortController();
    abortRef.current = controller;

    try {
      const res = await fetch('/api/ollama', {
        method: 'POST',
        signal: controller.signal,
        cache: 'no-store',
      });

      if (!res.ok || !res.body) throw new Error(`HTTP ${res.status}`);

      const reader = res.body.getReader();
      const decoder = new TextDecoder();

      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        const chunk = decoder.decode(value, { stream: true });
        setOutput(prev => {
          const next = prev + chunk;
          queueMicrotask(() => {
            if (boxRef.current) boxRef.current.scrollTop = boxRef.current.scrollHeight;
          });
          return next;
        });
      }
    } finally {
      setIsStreaming(false);
      abortRef.current = null;
    }
  }

  function stop() {
    abortRef.current?.abort();
  }

  useEffect(() => {
    run();
    return () => abortRef.current?.abort();
  }, []);

  const showLoader = isStreaming && output.length === 0;

  function handleClick() {
    if (isStreaming) stop();
    else run();
  }

  // sidebar handlers
  function addRow() {
    const v = parseFloat(String(tempInput).replace(',', '.'));
    if (Number.isNaN(v)) return;
    const row = { id: Date.now() + Math.random(), temp: v, ts: new Date() };
    setRows(prev => [row, ...prev]);
    setTempInput('');
  }
  function removeRow(id) {
    setRows(prev => prev.filter(r => r.id !== id));
  }
  function onInputKey(e) {
    if (e.key === 'Enter') addRow();
  }

  return (
    <main className="relative min-h-screen bg-gradient-to-b from-sky-50 via-zinc-50 to-white dark:from-zinc-950 dark:via-zinc-900 dark:to-zinc-900">
      {/* glow */}
      <div className="pointer-events-none absolute inset-0 -z-10">
        <div className="mx-auto mt-[-60px] h-60 w-60 rounded-full bg-sky-300/40 blur-3xl dark:bg-sky-500/20" />
      </div>

      {/* centered title */}
      <div className="w-full px-4 pt-10 text-center animate-fade-in">
        <div className="inline-flex items-center gap-3 rounded-2xl bg-white/70 px-4 py-2 shadow-sm ring-1 ring-zinc-200 backdrop-blur-md dark:bg-zinc-900/50 dark:ring-zinc-800">
          <CloudSun className="h-6 w-6 text-sky-600 dark:text-sky-400" />
          <h1 className="text-xl font-semibold tracking-wide">Weather Reporter</h1>
        </div>
      </div>

      {/* FIXED LEFT SIDEBAR (desktop) – doesn't affect centering */}
      <div className="hidden lg:block">
        <Sidebar
          className="fixed left-6 top-28 z-10 w-[280px]"
          tempInput={tempInput}
          setTempInput={setTempInput}
          addRow={addRow}
          onInputKey={onInputKey}
          rows={rows}
          removeRow={removeRow}
        />
      </div>

      {/* CENTERED report + button (always centered in viewport) */}
      <div className="w-full mt-6 px-4 flex justify-center">
        <div className="inline-flex items-start gap-3">
          <div
            ref={boxRef}
            className="min-h-[64px] max-h-[60vh] overflow-auto rounded-2xl border border-zinc-200 bg-white/80 p-4 font-mono text-sm text-zinc-800 shadow-md backdrop-blur-md whitespace-pre-wrap dark:border-zinc-800 dark:bg-zinc-900/60 dark:text-zinc-100"
            style={{ width: 720 }}
            aria-busy={showLoader}
            role="region"
          >
            {showLoader ? <Loader /> : (output || '')}
          </div>
          {/*}
          <button
            onClick={handleClick}
            aria-label={isStreaming ? 'Stop' : 'Try again'}
            className={[
              'h-[64px] w-[64px] rounded-2xl shadow-md transition',
              'flex items-center justify-center select-none',
              isStreaming
                ? 'bg-red-600 text-white hover:bg-red-700 active:scale-95 ring-2 ring-red-300/60 animate-pulse'
                : 'bg-zinc-900 text-white hover:bg-zinc-800 active:scale-95 ring-2 ring-zinc-300/40',
            ].join(' ')}
          >
            {isStreaming ? <Square className="h-6 w-6" /> : <RotateCcw className="h-6 w-6" />}
          </button>
          */}
        </div>
      </div>

      {/* MOBILE/TABLET SIDEBAR (below, keeps center centered) */}
      <div className="lg:hidden w-full px-4 mt-6">
        <Sidebar
          tempInput={tempInput}
          setTempInput={setTempInput}
          addRow={addRow}
          onInputKey={onInputKey}
          rows={rows}
          removeRow={removeRow}
        />
      </div>

      <style jsx>{`
        @keyframes fade-in {
          from { opacity: 0; transform: translateY(-6px); }
          to { opacity: 1; transform: translateY(0); }
        }
        .animate-fade-in { animation: fade-in 400ms ease-out both; }
      `}</style>
    </main>
  );
}

// replace your Sidebar with this one (icons already imported above)
function Sidebar({ className = '' }) {
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);
  const [err, setErr] = useState('');
  const [refreshing, setRefreshing] = useState(false);

  useEffect(() => {
    fetchOnce();
    const id = setInterval(fetchOnce, 30_000);
    return () => clearInterval(id);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  async function fetchOnce() {
    try {
      setRefreshing(true);
      const res = await fetch('/api/weather', { cache: 'no-store' });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const json = await res.json();
      setData(json);
      setErr('');
    } catch (e) {
      setErr(String(e));
    } finally {
      setLoading(false);
      setRefreshing(false);
    }
  }

  const temp = toNum(data?.temperature);
  const humidity = toNum(data?.humidity);
  const pressure_hpa = data?.pressure != null ? (Number(data.pressure) / 100) : null; // Pa → hPa
  const eco2 = toInt(data?.eco2);
  const tvoc = toInt(data?.tvoc);

  const updated = data?.timestamp ? new Date(data.timestamp) : null;

  return (
    <aside
      className={[
        'rounded-2xl border border-zinc-200 bg-white/80 p-4 shadow-md backdrop-blur-md',
        'dark:border-zinc-800 dark:bg-zinc-900/60',
        className,
      ].join(' ')}
    >
      <div className="mb-2 flex items-center justify-between">
        <div className="text-sm font-medium text-zinc-700 dark:text-zinc-200">
          Stats {data?.device_id ? <span className="text-zinc-400">· {data.device_id}</span> : null}
        </div>
        <button
          onClick={fetchOnce}
          title="Refresh"
          className={[
            'inline-flex h-8 w-8 items-center justify-center rounded-md',
            'text-zinc-600 hover:bg-zinc-200/60 dark:text-zinc-300 dark:hover:bg-zinc-800/60',
            refreshing ? 'animate-spin-slow' : ''
          ].join(' ')}
          aria-label="Refresh"
        >
          <RotateCcw className="h-4 w-4" />
        </button>
      </div>

      {/* rows */}
      <div className="space-y-2">
        <StaticRow
          icon={<Thermometer className="h-4 w-4 text-orange-500" />}
          label="Temperature"
          value={fmt1(temp)}
          unit="°C"
          loading={loading}
        />
        <StaticRow
          icon={<Droplets className="h-4 w-4 text-sky-600" />}
          label="Humidity"
          value={fmt1(humidity)}
          unit="%"
          loading={loading}
        />
        <StaticRow
          icon={<Gauge className="h-4 w-4 text-violet-600" />}
          label="Pressure"
          value={fmt0(pressure_hpa)}
          unit="hPa"
          loading={loading}
        />
        <StaticRow
          icon={<Wind className="h-4 w-4 text-emerald-600" />}
          label="eCO2"
          value={fmt0(eco2)}
          unit="ppm"
          loading={loading}
        />
        <StaticRow
          icon={<CloudFog className="h-4 w-4 text-fuchsia-600" />}
          label="TVOC"
          value={fmt0(tvoc)}
          unit="ppb"
          loading={loading}
        />
      </div>

      <div className="mt-3 text-[11px] text-zinc-500">
        {err
          ? <span className="text-red-600">Error: {err}</span>
          : updated
            ? <>Updated {updated.toLocaleString()}</>
            : loading
              ? 'Loading…'
              : '—'}
      </div>

      <style jsx>{`
        .animate-spin-slow { animation: spin 1.2s linear infinite; }
        @keyframes spin { to { transform: rotate(360deg); } }
      `}</style>
    </aside>
  );
}

function StaticRow({ icon, label, value, unit, loading }) {
  return (
    <div className="flex items-center justify-between rounded-lg border border-zinc-300/70 bg-white/90 px-3 py-2 dark:border-zinc-700/70 dark:bg-zinc-900/70">
      <div className="flex items-center gap-2">
        <div className="shrink-0">{icon}</div>
        <span className="text-sm text-zinc-700 dark:text-zinc-200">{label}</span>
      </div>
      <span className="text-sm font-semibold text-zinc-900 dark:text-zinc-100">
        {loading ? '—' : value ?? '—'}
        {!loading && value != null ? (
          <span className="ml-1 text-xs font-normal text-zinc-500 dark:text-zinc-400">{unit}</span>
        ) : null}
      </span>
    </div>
  );
}

function toNum(v) {
  if (v == null) return null;
  const n = Number(v);
  return Number.isFinite(n) ? n : null;
}
function toInt(v) {
  if (v == null) return null;
  const n = parseInt(v, 10);
  return Number.isFinite(n) ? n : null;
}
function fmt0(n) {
  if (n == null) return null;
  return Number(n).toFixed(0);
}
function fmt1(n) {
  if (n == null) return null;
  const x = Number(n);
  return Math.abs(x % 1) < 0.05 ? x.toFixed(0) : x.toFixed(1);
}

function Loader() {
  return (
    <div aria-live="polite" className="inline-flex items-center gap-2 text-zinc-500">
      <Dot />
      <Dot delay="0.18s" />
      <Dot delay="0.36s" />
      <style jsx>{`
        @keyframes pulse {
          0% { opacity: 0.2; transform: scale(0.85); }
          50% { opacity: 1; transform: scale(1); }
          100% { opacity: 0.2; transform: scale(0.85); }
        }
      `}</style>
    </div>
  );
}

function Dot({ delay = '0s' }) {
  return (
    <span
      style={{
        width: 6,
        height: 6,
        borderRadius: '50%',
        background: '#8f8f8f',
        display: 'inline-block',
        animation: 'pulse 1s infinite',
        animationDelay: delay,
      }}
    />
  );
}