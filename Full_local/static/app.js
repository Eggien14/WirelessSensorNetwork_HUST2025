// Global Variables
const API_BASE = '';  // Same origin
let relaysData = [];
let selectedRelays = [];
let thresholds = {};
let sensorData = [];
let autoRefreshInterval = null;
let systemRunning = false;
let currentSensorDetail = null; // Lưu thông tin sensor đang xem

// ==================== Initialization ====================

document.addEventListener('DOMContentLoaded', async () => {
    console.log('Khởi động ứng dụng...');
    
    // Load system status first
    await loadSystemStatus();
    
    // Load initial data
    await loadRelays();
    await loadGeneralData();  // Load dữ liệu ngay khi khởi động
    await loadThresholds();
    
    // Setup auto refresh if system is running
    setupAutoRefresh();
});

// ==================== System State Management ====================

async function loadSystemStatus() {
    try {
        const response = await fetch(`${API_BASE}/api/status`);
        const data = await response.json();
        
        if (data.success) {
            systemRunning = data.running;
            selectedRelays = data.selected_relays || [];
            
            // Restore total cycle value
            const totalCycleInput = document.getElementById('totalCycle');
            if (totalCycleInput && data.total_cycle) {
                totalCycleInput.value = data.total_cycle;
            }
            
            updateUIState();
        }
    } catch (error) {
        console.error('Lỗi tải trạng thái:', error);
    }
}

function updateUIState() {
    const startStopBtn = document.getElementById('startStopBtn');
    const refreshRelayBtn = document.getElementById('refreshRelayBtn');
    const totalCycleInput = document.getElementById('totalCycle');
    
    if (systemRunning) {
        // Đang chạy: Hiển thị STOP màu đỏ
        startStopBtn.textContent = 'STOP HỆ THỐNG';
        startStopBtn.classList.remove('btn-success');
        startStopBtn.classList.add('btn-danger');
        
        // Disable refresh relay button
        if (refreshRelayBtn) {
            refreshRelayBtn.disabled = true;
            refreshRelayBtn.style.opacity = '0.5';
        }
        
        // Disable total cycle input
        if (totalCycleInput) {
            totalCycleInput.disabled = true;
        }
        
        // Disable all checkboxes and delta inputs
        document.querySelectorAll('.relay-checkbox').forEach(cb => {
            cb.disabled = true;
            cb.classList.add('disabled-checkbox');
        });
        
        document.querySelectorAll('.delta-input').forEach(input => {
            input.disabled = true;
            input.classList.add('disabled-input');
        });
    } else {
        // Đang dừng: Hiển thị START màu xanh
        startStopBtn.textContent = 'START HỆ THỐNG';
        startStopBtn.classList.remove('btn-danger');
        startStopBtn.classList.add('btn-success');
        
        // Enable refresh relay button
        if (refreshRelayBtn) {
            refreshRelayBtn.disabled = false;
            refreshRelayBtn.style.opacity = '1';
        }
        
        // Enable total cycle input
        if (totalCycleInput) {
            totalCycleInput.disabled = false;
        }
        
        // Enable all checkboxes and delta inputs
        document.querySelectorAll('.relay-checkbox').forEach(cb => {
            cb.disabled = false;
            cb.classList.remove('disabled-checkbox');
        });
        
        document.querySelectorAll('.delta-input').forEach(input => {
            input.disabled = false;
            input.classList.remove('disabled-input');
        });
    }
}

function setupAutoRefresh() {
    // Clear existing interval
    if (autoRefreshInterval) {
        clearInterval(autoRefreshInterval);
    }
    
    // Auto refresh every 5 seconds - LUÔN CHẠY (không phụ thuộc systemRunning)
    autoRefreshInterval = setInterval(() => {
        // Refresh data on visible dashboard
        if (!document.getElementById('general-dashboard').classList.contains('hidden')) {
            loadGeneralData();
        }
    }, 5000); // 5 seconds
}

// ==================== Navigation ====================

function showDashboard(view) {
    // Hide all dashboards
    document.getElementById('manager-dashboard').classList.add('hidden');
    document.getElementById('general-dashboard').classList.add('hidden');
    document.getElementById('detail-dashboard').classList.add('hidden');
    
    // Update nav buttons
    document.querySelectorAll('.nav-btn').forEach(btn => btn.classList.remove('active'));
    
    // Show selected dashboard
    if (view === 'manager') {
        document.getElementById('manager-dashboard').classList.remove('hidden');
        event.target.classList.add('active');
        loadRelays();
    } else if (view === 'general') {
        document.getElementById('general-dashboard').classList.remove('hidden');
        event.target.classList.add('active');
        loadGeneralData();
    }
}

function backToGeneral() {
    showDashboard('general');
    document.querySelectorAll('.nav-btn')[1].classList.add('active');
}

// ==================== Manager Dashboard ====================

async function loadRelays() {
    try {
        const response = await fetch(`${API_BASE}/api/relays`);
        const data = await response.json();
        
        if (data.success) {
            relaysData = data.relays;
            
            // QUAN TRỌNG: Lọc bỏ các relay ID không tồn tại trong database
            // (Tránh trường hợp selectedRelays chứa ID cũ sau khi migrate)
            const validRelayIds = relaysData.map(r => r.relay_id);
            selectedRelays = selectedRelays.filter(id => validRelayIds.includes(id));
            
            renderRelayGrid();
        } else {
            showAlert('error', 'Lỗi tải dữ liệu relay: ' + data.error);
        }
    } catch (error) {
        console.error('Lỗi:', error);
        showAlert('error', 'Không thể kết nối tới server');
    }
}

function renderRelayGrid() {
    const grid = document.getElementById('relay-grid');
    
    if (relaysData.length === 0) {
        grid.innerHTML = '<div class="loading">Chưa có relay nào đăng ký. Vui lòng đợi Gateway gửi tin nhắn Advertise.</div>';
        return;
    }
    
    grid.innerHTML = relaysData.map(relay => {
        const isSelected = selectedRelays.includes(relay.relay_id);
        const isDisabled = systemRunning ? 'disabled' : '';
        const disabledClass = systemRunning ? 'disabled-input' : '';
        const disabledCheckboxClass = systemRunning ? 'disabled-checkbox' : '';
        const disabledBtnClass = systemRunning ? 'disabled-btn' : '';
        
        return `
        <div class="relay-item ${isSelected ? 'selected' : ''}" id="relay-${relay.relay_id}">
            <div class="relay-header">
                <input type="checkbox" 
                       class="relay-checkbox ${disabledCheckboxClass}" 
                       id="check-${relay.relay_id}"
                       ${isSelected ? 'checked' : ''}
                       ${isDisabled}
                       onchange="toggleRelay('${relay.relay_id}')">
                <span class="relay-id">Relay ${relay.relay_id}</span>
            </div>
            <div class="relay-info">
                Sensors: ${relay.sensor_ids.length > 0 ? relay.sensor_ids.join(', ') : 'Chưa có sensor'}
            </div>
            <div class="delta-input-group">
                <label>Δt (s):</label>
                <input type="number" 
                       class="delta-input ${disabledClass}" 
                       id="delta-${relay.relay_id}"
                       value="${relay.delta_t}"
                       min="1"
                       ${isDisabled}
                       onchange="updateDeltaT('${relay.relay_id}')">
            </div>
            <button class="btn-delete ${disabledBtnClass}" 
                    onclick="deleteRelay('${relay.relay_id}')"
                    ${isDisabled}
                    title="Xóa relay khỏi hệ thống">
                🗑️ Delete Relay
            </button>
        </div>
    `;
    }).join('');
}

function toggleRelay(relayId) {
    if (systemRunning) {
        showAlert('error', 'Không thể thay đổi relay khi hệ thống đang chạy');
        return;
    }
    
    const checkbox = document.getElementById(`check-${relayId}`);
    const relayItem = document.getElementById(`relay-${relayId}`);
    
    if (checkbox.checked) {
        relayItem.classList.add('selected');
        if (!selectedRelays.includes(relayId)) {
            selectedRelays.push(relayId);
        }
    } else {
        relayItem.classList.remove('selected');
        selectedRelays = selectedRelays.filter(id => id !== relayId);
    }
    
    console.log('Selected relays:', selectedRelays);
}

async function deleteRelay(relayId) {
    if (systemRunning) {
        showAlert('error', 'Không thể xóa relay khi hệ thống đang chạy');
        return;
    }
    
    // Popup xác nhận
    const confirmed = confirm(`Bạn có chắc chắn muốn xóa relay ${relayId} khỏi hệ thống?\n\nRelay này sẽ bị xóa hoàn toàn khỏi database và chỉ có thể đăng ký lại khi nhận tin nhắn Advertise mới.`);
    
    if (!confirmed) {
        return;
    }
    
    try {
        const response = await fetch(`${API_BASE}/api/relay/${relayId}`, {
            method: 'DELETE'
        });
        
        const data = await response.json();
        
        if (data.success) {
            showAlert('success', `Đã xóa relay ${relayId}`);
            
            // Xóa khỏi selectedRelays nếu đang được chọn
            selectedRelays = selectedRelays.filter(id => id !== relayId);
            
            // Reload danh sách relay
            await loadRelays();
        } else {
            showAlert('error', 'Lỗi: ' + data.error);
        }
    } catch (error) {
        console.error('Lỗi xóa relay:', error);
        showAlert('error', 'Không thể xóa relay');
    }
}

async function updateDeltaT(relayId) {
    if (systemRunning) {
        showAlert('error', 'Không thể thay đổi Δt khi hệ thống đang chạy');
        return;
    }
    
    const deltaInput = document.getElementById(`delta-${relayId}`);
    const deltaT = parseInt(deltaInput.value);
    
    if (deltaT < 1) {
        showAlert('error', 'Chu kỳ phải >= 1 phút');
        return;
    }
    
    try {
        const response = await fetch(`${API_BASE}/api/update_cycle`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                relay_id: relayId,
                delta_t: deltaT
            })
        });
        
        const data = await response.json();
        
        if (data.success) {
            console.log(`Cập nhật Δt Relay ${relayId}: ${deltaT} phút`);
        } else {
            showAlert('error', 'Lỗi cập nhật: ' + data.error);
        }
    } catch (error) {
        console.error('Lỗi:', error);
        showAlert('error', 'Không thể cập nhật chu kỳ');
    }
}

async function toggleSystem() {
    if (systemRunning) {
        // Stop system
        await stopSystem();
    } else {
        // Start system
        await startSystem();
    }
}

async function startSystem() {
    if (selectedRelays.length === 0) {
        showAlert('error', 'Vui lòng chọn ít nhất một relay trước khi START');
        return;
    }
    
    // Lấy giá trị T (Chu kỳ tổng)
    const totalCycleInput = document.getElementById('totalCycle');
    const totalCycle = parseInt(totalCycleInput?.value) || 120;
    
    if (totalCycle < 1) {
        showAlert('error', 'Chu kỳ tổng (T) phải lớn hơn 0');
        return;
    }
    
    try {
        const response = await fetch(`${API_BASE}/api/start`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                selected_relays: selectedRelays,
                total_cycle: totalCycle
            })
        });
        
        const data = await response.json();
        
        if (data.success) {
            systemRunning = true;
            updateUIState();
            setupAutoRefresh();
            showAlert('success', `Đã khởi động hệ thống với ${selectedRelays.length} relay, T=${totalCycle}s`);
        } else {
            showAlert('error', 'Lỗi: ' + data.error);
        }
    } catch (error) {
        console.error('Lỗi:', error);
        showAlert('error', 'Không thể khởi động hệ thống');
    }
}

async function stopSystem() {
    try {
        const response = await fetch(`${API_BASE}/api/stop`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' }
        });
        
        const data = await response.json();
        
        if (data.success) {
            systemRunning = false;
            updateUIState();
            showAlert('success', 'Đã dừng hệ thống');
        } else {
            showAlert('error', 'Lỗi: ' + data.error);
        }
    } catch (error) {
        console.error('Lỗi:', error);
        showAlert('error', 'Không thể dừng hệ thống');
    }
}

// ==================== Threshold Settings ====================

async function loadThresholds() {
    try {
        const response = await fetch(`${API_BASE}/api/thresholds`);
        const data = await response.json();
        
        if (data.success) {
            thresholds = data.thresholds;
            
            // Fill input fields
            document.getElementById('temp_min').value = thresholds.temp_min;
            document.getElementById('temp_max').value = thresholds.temp_max;
            document.getElementById('humid_min').value = thresholds.humid_min;
            document.getElementById('humid_max').value = thresholds.humid_max;
            document.getElementById('soil_min').value = thresholds.soil_min;
            document.getElementById('soil_max').value = thresholds.soil_max;
        }
    } catch (error) {
        console.error('Lỗi tải ngưỡng:', error);
    }
}

async function saveThresholds() {
    const newThresholds = {
        temp_min: parseFloat(document.getElementById('temp_min').value),
        temp_max: parseFloat(document.getElementById('temp_max').value),
        humid_min: parseFloat(document.getElementById('humid_min').value),
        humid_max: parseFloat(document.getElementById('humid_max').value),
        soil_min: parseFloat(document.getElementById('soil_min').value),
        soil_max: parseFloat(document.getElementById('soil_max').value)
    };
    
    try {
        const response = await fetch(`${API_BASE}/api/thresholds`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(newThresholds)
        });
        
        const data = await response.json();
        
        if (data.success) {
            thresholds = data.thresholds;
            showAlert('success', 'Đã lưu ngưỡng cảnh báo');
        } else {
            showAlert('error', 'Lỗi: ' + data.error);
        }
    } catch (error) {
        console.error('Lỗi:', error);
        showAlert('error', 'Không thể lưu ngưỡng');
    }
}

// ==================== General Dashboard ====================

async function loadGeneralData() {
    try {
        const response = await fetch(`${API_BASE}/api/data`);
        const data = await response.json();
        
        if (data.success) {
            sensorData = data.data;
            renderGeneralTable();
        } else {
            showAlert('error', 'Lỗi tải dữ liệu: ' + data.error);
        }
    } catch (error) {
        console.error('Lỗi:', error);
        showAlert('error', 'Không thể tải dữ liệu');
    }
}

function renderGeneralTable() {
    const container = document.getElementById('general-table-container');
    
    if (sensorData.length === 0) {
        container.innerHTML = '<div class="loading">Chưa có dữ liệu. Vui lòng đợi Gateway gửi dữ liệu.</div>';
        return;
    }
    
    // Filter only selected relays (ĐÚNG LOGIC)
    const filteredData = sensorData.filter(item => 
        selectedRelays.includes(item.relay_id)
    );
    
    if (filteredData.length === 0) {
        container.innerHTML = '<div class="loading">Chưa có dữ liệu từ các relay đã chọn. Vui lòng chọn relay ở trang Quản Lý.</div>';
        return;
    }
    
    let tableHTML = `
        <table class="data-table">
            <thead>
                <tr>
                    <th>Relay</th>
                    <th>Sensor</th>
                    <th>Nhiệt độ (°C)</th>
                    <th>Độ ẩm (%)</th>
                    <th>Độ ẩm đất (%)</th>
                    <th>Thời gian</th>
                    <th>Trạng thái</th>
                </tr>
            </thead>
            <tbody>
    `;
    
    filteredData.forEach(item => {
        const isRelay = item.relay_id === item.sensor_id;
        const status = checkStatus(item);
        
        tableHTML += `
            <tr class="${isRelay ? 'relay-row' : ''}">
                <td class="${isRelay ? 'relay-id-col' : ''}">${item.relay_id}</td>
                <td>
                    <a class="sensor-link" onclick="showSensorDetail('${item.relay_id}', '${item.sensor_id}')">
                        ${item.sensor_id}
                    </a>
                </td>
                <td>${parseFloat(item.temp).toFixed(1)}</td>
                <td>${parseFloat(item.humid).toFixed(1)}</td>
                <td>${parseFloat(item.soil).toFixed(1)}</td>
                <td>${item.timestamp || 'N/A'}</td>
                <td class="${status.class}">${status.text}</td>
            </tr>
        `;
    });
    
    tableHTML += '</tbody></table>';
    container.innerHTML = tableHTML;
}

function checkStatus(item) {
    const temp = parseFloat(item.temp);
    const humid = parseFloat(item.humid);
    const soil = parseFloat(item.soil);
    
    const warnings = [];
    
    if (temp < thresholds.temp_min || temp > thresholds.temp_max) {
        warnings.push('Nhiệt độ');
    }
    if (humid < thresholds.humid_min || humid > thresholds.humid_max) {
        warnings.push('Độ ẩm');
    }
    if (soil < thresholds.soil_min || soil > thresholds.soil_max) {
        warnings.push('Độ ẩm đất');
    }
    
    if (warnings.length === 0) {
        return { text: 'NORMAL', class: 'status-normal' };
    } else if (warnings.length === 1) {
        return { text: `CẢNH BÁO: ${warnings[0]}`, class: 'status-warning' };
    } else {
        return { text: `NGUY HIỂM: ${warnings.join(', ')}`, class: 'status-danger' };
    }
}

// ==================== Sensor Detail Dashboard ====================

async function showSensorDetail(relayId, sensorId) {
    try {
        const timeRange = document.getElementById('timeRangeSelect')?.value || '24hour';
        const response = await fetch(`${API_BASE}/api/sensor/${relayId}/${sensorId}?time_range=${timeRange}`);
        const data = await response.json();
        
        if (data.success) {
            currentSensorDetail = { relay_id: relayId, sensor_id: sensorId };
            renderSensorDetail(data);
            
            // Switch to detail view
            document.getElementById('general-dashboard').classList.add('hidden');
            document.getElementById('detail-dashboard').classList.remove('hidden');
        } else {
            showAlert('error', 'Lỗi tải chi tiết: ' + data.error);
        }
    } catch (error) {
        console.error('Lỗi:', error);
        showAlert('error', 'Không thể tải chi tiết sensor');
    }
}

async function changeTimeRange() {
    if (currentSensorDetail) {
        await showSensorDetail(currentSensorDetail.relay_id, currentSensorDetail.sensor_id);
    }
}

function renderSensorDetail(data) {
    const title = data.is_relay ? 
        `Chi Tiết Relay ${data.relay_id}` : 
        `Chi Tiết Sensor ${data.sensor_id}`;
    
    document.getElementById('detail-title').textContent = title;
    
    // Render info cards
    let infoHTML = `
        <div class="info-card">
            <h3>ID</h3>
            <p>${data.sensor_id}</p>
        </div>
    `;
    
    if (data.is_relay) {
        const sensors = data.managed_sensors.length > 0 ? 
            data.managed_sensors.join(', ') : 'Không có sensor';
        infoHTML += `
            <div class="info-card">
                <h3>Sensors Quản Lý</h3>
                <p>${sensors}</p>
            </div>
        `;
    } else {
        infoHTML += `
            <div class="info-card">
                <h3>Relay Quản Lý</h3>
                <p>${data.relay_id}</p>
            </div>
        `;
    }
    
    // Add latest data
    if (data.history.length > 0) {
        const latest = data.history[0];
        const status = checkStatus(latest);
        
        infoHTML += `
            <div class="info-card">
                <h3>Trạng Thái</h3>
                <p class="${status.class}">${status.text}</p>
            </div>
            <div class="info-card">
                <h3>Lần Đo Gần Nhất</h3>
                <p>${latest.timestamp || 'N/A'}</p>
            </div>
        `;
    }
    
    document.getElementById('sensor-info').innerHTML = infoHTML;
    
    // Render charts
    renderCharts(data.history);
}

let tempChart, humidChart, soilChart;

function renderCharts(history) {
    // Destroy existing charts
    if (tempChart) tempChart.destroy();
    if (humidChart) humidChart.destroy();
    if (soilChart) soilChart.destroy();
    
    if (history.length === 0) {
        document.querySelector('.chart-wrapper').innerHTML = '<p>Chưa có dữ liệu để hiển thị biểu đồ</p>';
        return;
    }
    
    console.log('📊 Rendering charts with thresholds:', thresholds);
    console.log('📊 History data points:', history.length);
    
    // Prepare data (reverse to show chronological order)
    const labels = history.map(h => h.timestamp || 'N/A').reverse();
    const tempData = history.map(h => parseFloat(h.temp)).reverse();
    const humidData = history.map(h => parseFloat(h.humid)).reverse();
    const soilData = history.map(h => parseFloat(h.soil)).reverse();
    
    const chartConfig = {
        type: 'line',
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: true,
                    position: 'top'
                }
            },
            scales: {
                y: {
                    beginAtZero: false
                }
            }
        }
    };
    
    // Temperature Chart with thresholds
    tempChart = new Chart(document.getElementById('tempChart'), {
        ...chartConfig,
        data: {
            labels: labels,
            datasets: [
                {
                    label: 'Nhiệt Độ (°C)',
                    data: tempData,
                    borderColor: 'rgb(255, 99, 132)',
                    backgroundColor: 'rgba(255, 99, 132, 0.1)',
                    tension: 0,
                    borderWidth: 2
                },
                {
                    label: 'Ngưỡng Trên',
                    data: new Array(labels.length).fill(thresholds.temp_max),
                    borderColor: 'rgba(255, 0, 0, 0.7)',
                    borderDash: [5, 5],
                    borderWidth: 2,
                    pointRadius: 0,
                    fill: false
                },
                {
                    label: 'Ngưỡng Dưới',
                    data: new Array(labels.length).fill(thresholds.temp_min),
                    borderColor: 'rgba(0, 0, 255, 0.7)',
                    borderDash: [5, 5],
                    borderWidth: 2,
                    pointRadius: 0,
                    fill: false
                }
            ]
        }
    });
    
    // Humidity Chart with thresholds
    humidChart = new Chart(document.getElementById('humidChart'), {
        ...chartConfig,
        data: {
            labels: labels,
            datasets: [
                {
                    label: 'Độ Ẩm Không Khí (%)',
                    data: humidData,
                    borderColor: 'rgb(54, 162, 235)',
                    backgroundColor: 'rgba(54, 162, 235, 0.1)',
                    tension: 0,
                    borderWidth: 2
                },
                {
                    label: 'Ngưỡng Trên',
                    data: new Array(labels.length).fill(thresholds.humid_max),
                    borderColor: 'rgba(255, 0, 0, 0.7)',
                    borderDash: [5, 5],
                    borderWidth: 2,
                    pointRadius: 0,
                    fill: false
                },
                {
                    label: 'Ngưỡng Dưới',
                    data: new Array(labels.length).fill(thresholds.humid_min),
                    borderColor: 'rgba(0, 0, 255, 0.7)',
                    borderDash: [5, 5],
                    borderWidth: 2,
                    pointRadius: 0,
                    fill: false
                }
            ]
        }
    });
    
    // Soil Chart with thresholds
    soilChart = new Chart(document.getElementById('soilChart'), {
        ...chartConfig,
        data: {
            labels: labels,
            datasets: [
                {
                    label: 'Độ Ẩm Đất (%)',
                    data: soilData,
                    borderColor: 'rgb(75, 192, 192)',
                    backgroundColor: 'rgba(75, 192, 192, 0.1)',
                    tension: 0,
                    borderWidth: 2
                },
                {
                    label: 'Ngưỡng Trên',
                    data: new Array(labels.length).fill(thresholds.soil_max),
                    borderColor: 'rgba(255, 0, 0, 0.7)',
                    borderDash: [5, 5],
                    borderWidth: 2,
                    pointRadius: 0,
                    fill: false
                },
                {
                    label: 'Ngưỡng Dưới',
                    data: new Array(labels.length).fill(thresholds.soil_min),
                    borderColor: 'rgba(0, 0, 255, 0.7)',
                    borderDash: [5, 5],
                    borderWidth: 2,
                    pointRadius: 0,
                    fill: false
                }
            ]
        }
    });
}

// ==================== Utility Functions ====================

function showAlert(type, message) {
    const alertClass = type === 'error' ? 'alert-error' : 
                       type === 'success' ? 'alert-success' : 'alert-info';
    
    const alertDiv = document.createElement('div');
    alertDiv.className = `alert ${alertClass}`;
    alertDiv.textContent = message;
    
    const container = document.querySelector('.container');
    container.insertBefore(alertDiv, container.firstChild);
    
    // Auto remove after 5 seconds
    setTimeout(() => {
        alertDiv.remove();
    }, 5000);
}
