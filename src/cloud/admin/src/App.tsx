import { Routes, Route, Navigate } from "react-router-dom";
import { useState, useEffect } from "react";
import { AdminLayout } from "./components/AdminLayout";
import { Dashboard } from "./pages/Dashboard";
import { Users } from "./pages/Users";
import { UserDetail } from "./pages/UserDetail";
import { Devices } from "./pages/Devices";
import { DeviceDetail } from "./pages/DeviceDetail";
import { Sessions } from "./pages/Sessions";
import { Monitoring } from "./pages/Monitoring";
import { Logs } from "./pages/Logs";
import { AccessDenied } from "./pages/AccessDenied";
import { Login } from "./pages/Login";
import type { AdminUser } from "./lib/types";
import { api, getStoredSession, clearSession } from "./lib/api";

function App() {
  const [user, setUser] = useState<AdminUser | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    // Check if user is authenticated and is admin
    checkAuth();
  }, []);

  async function checkAuth() {
    try {
      const session = getStoredSession();
      if (!session?.accessToken) {
        setLoading(false);
        return;
      }

      const data = await api.getMe();
      if (data.user.isAdmin) {
        setUser(data.user);
      } else {
        // User is authenticated but not admin
        setUser({ ...data.user, isAdmin: false } as AdminUser);
      }
    } catch (error) {
      console.error("Auth check failed:", error);
      clearSession();
    } finally {
      setLoading(false);
    }
  }

  function handleLogout() {
    clearSession();
    setUser(null);
  }

  if (loading) {
    return (
      <div className="min-h-screen bg-admin-bg flex items-center justify-center">
        <div className="text-admin-text-secondary">Loading...</div>
      </div>
    );
  }

  if (!user) {
    return <Login onLogin={checkAuth} />;
  }

  if (!user.isAdmin) {
    return <AccessDenied user={user} onLogout={handleLogout} />;
  }

  return (
    <AdminLayout user={user} onLogout={handleLogout}>
      <Routes>
        <Route path="/" element={<Dashboard />} />
        <Route path="/users" element={<Users />} />
        <Route path="/users/:id" element={<UserDetail />} />
        <Route path="/devices" element={<Devices />} />
        <Route path="/devices/:id" element={<DeviceDetail />} />
        <Route path="/sessions" element={<Sessions />} />
        <Route path="/monitoring" element={<Monitoring />} />
        <Route path="/logs" element={<Logs />} />
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </AdminLayout>
  );
}

export default App;

