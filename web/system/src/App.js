import React, { useState, useEffect } from 'react';

function App() {
  const [systemInfo, setSystemInfo] = useState(null);
  const [systemStatus, setSystemStatus] = useState(null);
  const [wifiInfo, setWifiInfo] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetchSystemData();
    const interval = setInterval(fetchSystemStatus, 5000);
    return () => clearInterval(interval);
  }, []);

  const fetchSystemData = async () => {
    try {
      const [infoRes, statusRes, wifiRes] = await Promise.all([
        fetch('/api/system/info'),
        fetch('/api/system/status'),
        fetch('/api/system/wifi')
      ]);
      
      const info = await infoRes.json();
      const status = await statusRes.json();
      const wifi = await wifiRes.json();
      
      setSystemInfo(info);
      setSystemStatus(status);
      setWifiInfo(wifi);
      setLoading(false);
    } catch (error) {
      console.error('Error fetching system data:', error);
      setLoading(false);
    }
  };

  const fetchSystemStatus = async () => {
    try {
      const response = await fetch('/api/system/status');
      const data = await response.json();
      setSystemStatus(data);
    } catch (error) {
      console.error('Error fetching system status:', error);
    }
  };

  const handleRestart = async () => {
    if (confirm('Are you sure you want to restart the device?')) {
      try {
        await fetch('/api/system/restart', { method: 'POST' });
        alert('Device is restarting...');
      } catch (error) {
        console.error('Error restarting device:', error);
      }
    }
  };

  const handleButtonClick = () => {
    alert('You clicked my button!');
  };

  const formatUptime = (seconds) => {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    return `${hours}h ${minutes}m ${secs}s`;
  };

  const formatBytes = (bytes) => {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1048576) return (bytes / 1024).toFixed(2) + ' KB';
    return (bytes / 1048576).toFixed(2) + ' MB';
  };

  if (loading) {
    return (
      <div className="container">
        <h1>Loading...</h1>
      </div>
    );
  }

  return (
    <div className="container">
      <h1>System Interface</h1>
      
      {systemInfo && (
        <div className="info-section">
          <h2>Device Information</h2>
          <div className="info-grid">
            <div className="info-item">
              <span className="label">Device:</span>
              <span className="value">{systemInfo.device}</span>
            </div>
            <div className="info-item">
              <span className="label">Chip Model:</span>
              <span className="value">{systemInfo.chipModel}</span>
            </div>
            <div className="info-item">
              <span className="label">CPU Frequency:</span>
              <span className="value">{systemInfo.cpuFreqMHz} MHz</span>
            </div>
            <div className="info-item">
              <span className="label">Flash Size:</span>
              <span className="value">{formatBytes(systemInfo.flashSize)}</span>
            </div>
          </div>
        </div>
      )}

      {systemStatus && (
        <div className="info-section">
          <h2>System Status</h2>
          <div className="info-grid">
            <div className="info-item">
              <span className="label">Uptime:</span>
              <span className="value">{formatUptime(systemStatus.uptime)}</span>
            </div>
            <div className="info-item">
              <span className="label">Free Heap:</span>
              <span className="value">{formatBytes(systemStatus.freeHeap)}</span>
            </div>
            <div className="info-item">
              <span className="label">Heap Size:</span>
              <span className="value">{formatBytes(systemStatus.heapSize)}</span>
            </div>
          </div>
        </div>
      )}

      {wifiInfo && (
        <div className="info-section">
          <h2>WiFi Information</h2>
          <div className="info-grid">
            <div className="info-item">
              <span className="label">SSID:</span>
              <span className="value">{wifiInfo.ssid}</span>
            </div>
            <div className="info-item">
              <span className="label">IP Address:</span>
              <span className="value">{wifiInfo.ip}</span>
            </div>
            <div className="info-item">
              <span className="label">Connected Clients:</span>
              <span className="value">{wifiInfo.connectedClients}</span>
            </div>
          </div>
        </div>
      )}

      <div className="button-group">
        <button onClick={handleButtonClick}>Click Me!</button>
        <button onClick={handleRestart} className="btn-danger">
          Restart Device
        </button>
      </div>
    </div>
  );
}

export default App;

