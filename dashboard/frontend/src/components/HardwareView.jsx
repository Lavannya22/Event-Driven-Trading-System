export default function HardwareView({ hardware = {} }) {
  const {
    platform       = 'unknown',
    physical_cores = 0,
    logical_cpus   = 0,
    hyperthreading = false,
    isolated_cpus  = [],
    l1_kb          = 0,
    l2_kb          = 0,
    l3_kb          = 0,
    numa_nodes     = [],
    huge_pages_available = false,
    huge_page_size_kb    = 0,
    huge_pages_free      = 0,
    huge_pages_total     = 0,
    simd_level           = 'unknown',
    dpdk_available       = false,
  } = hardware

  return (
    <div className="space-y-8">

      {/* Platform banner */}
      <div className={`px-4 py-2 rounded-lg text-sm font-semibold ${
        platform.includes('WSL2')
          ? 'bg-yellow-900/40 text-yellow-300 border border-yellow-700'
          : 'bg-green-900/40 text-green-300 border border-green-700'
      }`}>
        {platform.includes('WSL2')
          ? '⚠ WSL2 detected — hardware fallbacks active. Authoritative measurements require bare-metal Linux.'
          : `✓ ${platform}`}
      </div>

      {/* CPU */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">CPU Topology</h3>
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <InfoCard label="Physical Cores" value={physical_cores} />
          <InfoCard label="Logical CPUs"   value={logical_cpus} />
          <InfoCard
            label="Hyperthreading"
            value={hyperthreading ? 'ENABLED' : 'disabled'}
            color={hyperthreading ? 'text-yellow-400' : 'text-green-400'}
          />
          <InfoCard
            label="Isolated CPUs"
            value={isolated_cpus.length > 0 ? isolated_cpus.join(', ') : 'none'}
            color={isolated_cpus.length > 0 ? 'text-green-400' : 'text-gray-500'}
          />
        </div>
        {hyperthreading && (
          <p className="text-xs text-yellow-600 mt-2">
            Disable HT in BIOS for authoritative latency measurements on hot-path cores.
          </p>
        )}
      </section>

      {/* Cache */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">Cache Hierarchy</h3>
        <div className="grid grid-cols-3 gap-4 max-w-sm">
          <CacheCard label="L1" kb={l1_kb} />
          <CacheCard label="L2" kb={l2_kb} />
          <CacheCard label="L3" kb={l3_kb} />
        </div>
      </section>

      {/* NUMA */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">NUMA Topology</h3>
        {numa_nodes.length === 0 ? (
          <p className="text-gray-600 text-sm">No NUMA data available.</p>
        ) : (
          <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
            {numa_nodes.map(n => (
              <div key={n.node_id} className="bg-gray-900 rounded-lg p-3">
                <div className="text-xs text-gray-500 mb-1">Node {n.node_id}</div>
                <div className="text-sm text-gray-300">
                  CPUs: {n.cpus?.join(', ') ?? '—'}
                </div>
                {n.memory_kb > 0 && (
                  <div className="text-xs text-gray-500 mt-1">
                    {(n.memory_kb / 1024 / 1024).toFixed(1)} GB
                  </div>
                )}
              </div>
            ))}
          </div>
        )}
      </section>

      {/* Huge Pages + SIMD + DPDK */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Hardware Features
        </h3>
        <div className="grid grid-cols-2 md:grid-cols-3 gap-4">
          <FeatureCard
            label="Huge Pages"
            available={huge_pages_available}
            detail={huge_pages_available
              ? `${huge_pages_free}/${huge_pages_total} free (${huge_page_size_kb} KB)`
              : 'not configured — run: sudo sysctl -w vm.nr_hugepages=512'}
          />
          <FeatureCard
            label="SIMD"
            available={simd_level !== 'scalar'}
            detail={`Best available: ${simd_level}`}
          />
          <FeatureCard
            label="DPDK"
            available={dpdk_available}
            detail={dpdk_available ? 'kernel-bypass transport ready' : 'unavailable on WSL2 / no DPDK NIC'}
          />
        </div>
      </section>

    </div>
  )
}

function InfoCard({ label, value, color = 'text-gray-200' }) {
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">{label}</div>
      <div className={`text-lg font-bold ${color}`}>{value}</div>
    </div>
  )
}

function CacheCard({ label, kb }) {
  const display = kb >= 1024
    ? `${(kb / 1024).toFixed(0)} MB`
    : kb > 0 ? `${kb} KB` : '—'
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">{label}</div>
      <div className="text-xl font-bold text-blue-400">{display}</div>
    </div>
  )
}

function FeatureCard({ label, available, detail }) {
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="flex items-center gap-2 mb-2">
        <span className={`w-2 h-2 rounded-full ${available ? 'bg-green-400' : 'bg-gray-600'}`} />
        <div className="text-sm font-semibold text-gray-200">{label}</div>
      </div>
      <div className="text-xs text-gray-500">{detail}</div>
    </div>
  )
}
