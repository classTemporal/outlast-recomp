package com.tapps.outrecomp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Lightweight multi-touch virtual Xbox 360 gamepad overlay for testing on Android.
 * Maps directly to XInput buttons, triggers, and analog sticks.
 */
public class VirtualGamepadView extends View {
    public interface Listener {
        void onButtonChanged(int xinputMask, boolean down);
        void onAxesChanged(int lt, int rt, int lx, int ly, int rx, int ry);
    }

    private static final int BTN_DPAD_UP    = 0x0001;
    private static final int BTN_DPAD_DOWN  = 0x0002;
    private static final int BTN_DPAD_LEFT  = 0x0004;
    private static final int BTN_DPAD_RIGHT = 0x0008;
    private static final int BTN_START      = 0x0010;
    private static final int BTN_BACK       = 0x0020;
    private static final int BTN_L3         = 0x0040;
    private static final int BTN_R3         = 0x0080;
    private static final int BTN_LB         = 0x0100;
    private static final int BTN_RB         = 0x0200;
    private static final int BTN_A          = 0x1000;
    private static final int BTN_B          = 0x2000;
    private static final int BTN_X          = 0x4000;
    private static final int BTN_Y          = 0x8000;

    private static final int TRIG_LT = -1;
    private static final int TRIG_RT = -2;

    private static class Control {
        final String label;
        final int code;
        final boolean circle;
        final RectF rect = new RectF();
        int activePointers = 0;

        Control(String label, int code, boolean circle) {
            this.label = label;
            this.code = code;
            this.circle = circle;
        }
    }

    private static class Stick {
        final String label;
        float cx, cy, baseR, knobR;
        float normX = 0f, normY = 0f;
        int pointerId = -1;

        Stick(String label) {
            this.label = label;
        }
    }

    private final Listener listener;
    private final List<Control> controls = new ArrayList<>();
    private final Stick leftStick = new Stick("L");
    private final Stick rightStick = new Stick("R");
    private final RectF toggleRect = new RectF();
    private final Map<Integer, Control> pointerToControl = new HashMap<>();

    private boolean controlsVisible = true;
    private int togglePointerId = -1;
    private int lastLt = 0, lastRt = 0, lastLx = 0, lastLy = 0, lastRx = 0, lastRy = 0;

    private final Paint fillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint activeFillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint strokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    public VirtualGamepadView(Context context, Listener listener) {
        super(context);
        this.listener = listener;

        fillPaint.setColor(Color.argb(72, 20, 24, 28));
        fillPaint.setStyle(Paint.Style.FILL);

        activeFillPaint.setColor(Color.argb(145, 76, 175, 80));
        activeFillPaint.setStyle(Paint.Style.FILL);

        strokePaint.setColor(Color.argb(150, 220, 230, 240));
        strokePaint.setStyle(Paint.Style.STROKE);
        strokePaint.setStrokeWidth(3f);

        textPaint.setColor(Color.argb(220, 240, 245, 250));
        textPaint.setTextAlign(Paint.Align.CENTER);

        controls.add(new Control("LT", TRIG_LT, false));
        controls.add(new Control("LB", BTN_LB, false));
        controls.add(new Control("RT", TRIG_RT, false));
        controls.add(new Control("RB", BTN_RB, false));

        controls.add(new Control("^", BTN_DPAD_UP, false));
        controls.add(new Control("v", BTN_DPAD_DOWN, false));
        controls.add(new Control("<", BTN_DPAD_LEFT, false));
        controls.add(new Control(">", BTN_DPAD_RIGHT, false));

        controls.add(new Control("A", BTN_A, true));
        controls.add(new Control("B", BTN_B, true));
        controls.add(new Control("X", BTN_X, true));
        controls.add(new Control("Y", BTN_Y, true));

        controls.add(new Control("BACK", BTN_BACK, false));
        controls.add(new Control("START", BTN_START, false));
        controls.add(new Control("L3", BTN_L3, true));
        controls.add(new Control("R3", BTN_R3, true));
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        float u = Math.min(w, h) / 14f;
        float pad = u * 0.45f;
        textPaint.setTextSize(u * 0.48f);

        // Top-center toggle pill
        float tw = u * 2.2f, th = u * 0.8f;
        toggleRect.set(w * 0.5f - tw * 0.5f, pad * 0.6f, w * 0.5f + tw * 0.5f, pad * 0.6f + th);

        // Shoulder & trigger buttons
        float sw = u * 1.55f, sh = u * 0.9f;
        setControlRect(TRIG_LT, pad, pad, pad + sw, pad + sh);
        setControlRect(BTN_LB, pad + sw + pad * 0.4f, pad, pad + sw * 2 + pad * 0.4f, pad + sh);
        setControlRect(TRIG_RT, w - pad - sw, pad, w - pad, pad + sh);
        setControlRect(BTN_RB, w - pad - sw * 2 - pad * 0.4f, pad, w - pad - sw - pad * 0.4f, pad + sh);

        // Back / Start
        float mw = u * 1.6f, mh = u * 0.78f;
        setControlRect(BTN_BACK, w * 0.5f - mw - u * 0.8f, h - pad - mh, w * 0.5f - u * 0.8f, h - pad);
        setControlRect(BTN_START, w * 0.5f + u * 0.8f, h - pad - mh, w * 0.5f + mw + u * 0.8f, h - pad);

        // Left stick (bottom-left)
        leftStick.baseR = u * 1.75f;
        leftStick.knobR = u * 0.72f;
        leftStick.cx = pad + leftStick.baseR + u * 0.25f;
        leftStick.cy = h - pad - leftStick.baseR - u * 0.15f;

        // D-Pad (above left stick)
        float ds = u * 1.05f;
        float dcx = leftStick.cx;
        float dcy = leftStick.cy - leftStick.baseR - ds * 1.75f;
        setControlRect(BTN_DPAD_UP,    dcx - ds * 0.5f, dcy - ds * 1.5f, dcx + ds * 0.5f, dcy - ds * 0.5f);
        setControlRect(BTN_DPAD_DOWN,  dcx - ds * 0.5f, dcy + ds * 0.5f, dcx + ds * 0.5f, dcy + ds * 1.5f);
        setControlRect(BTN_DPAD_LEFT,  dcx - ds * 1.5f, dcy - ds * 0.5f, dcx - ds * 0.5f, dcy + ds * 0.5f);
        setControlRect(BTN_DPAD_RIGHT, dcx + ds * 0.5f, dcy - ds * 0.5f, dcx + ds * 1.5f, dcy + ds * 0.5f);

        // Right stick (bottom-right, inner)
        rightStick.baseR = u * 1.65f;
        rightStick.knobR = u * 0.68f;
        rightStick.cx = w - pad - rightStick.baseR - u * 0.25f;
        rightStick.cy = h - pad - rightStick.baseR - u * 0.15f;

        // A / B / X / Y diamond (above right stick)
        float br = u * 0.64f;
        float fcx = rightStick.cx;
        float fcy = rightStick.cy - rightStick.baseR - br * 2.7f;
        float off = br * 1.45f;
        setControlCircle(BTN_Y, fcx, fcy - off, br);
        setControlCircle(BTN_A, fcx, fcy + off, br);
        setControlCircle(BTN_X, fcx - off, fcy, br);
        setControlCircle(BTN_B, fcx + off, fcy, br);

        // L3 / R3 near bottom-inner
        float l3r = u * 0.55f;
        setControlCircle(BTN_L3, leftStick.cx + leftStick.baseR + l3r * 1.3f, h - pad - l3r, l3r);
        setControlCircle(BTN_R3, rightStick.cx - rightStick.baseR - l3r * 1.3f, h - pad - l3r, l3r);
    }

    private void setControlRect(int code, float l, float t, float r, float b) {
        for (Control c : controls) {
            if (c.code == code) {
                c.rect.set(l, t, r, b);
                return;
            }
        }
    }

    private void setControlCircle(int code, float cx, float cy, float r) {
        setControlRect(code, cx - r, cy - r, cx + r, cy + r);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        drawRoundBox(canvas, toggleRect, controlsVisible ? "HIDE PAD" : "SHOW PAD", false);
        if (!controlsVisible) return;

        for (Control c : controls) {
            boolean active = c.activePointers > 0;
            if (c.circle) {
                float cx = c.rect.centerX(), cy = c.rect.centerY(), r = c.rect.width() * 0.5f;
                canvas.drawCircle(cx, cy, r, active ? activeFillPaint : fillPaint);
                canvas.drawCircle(cx, cy, r, strokePaint);
                drawCenteredText(canvas, c.label, cx, cy);
            } else {
                drawRoundBox(canvas, c.rect, c.label, active);
            }
        }
        drawStick(canvas, leftStick);
        drawStick(canvas, rightStick);
    }

    private void drawRoundBox(Canvas canvas, RectF r, String label, boolean active) {
        float rad = Math.min(r.width(), r.height()) * 0.28f;
        canvas.drawRoundRect(r, rad, rad, active ? activeFillPaint : fillPaint);
        canvas.drawRoundRect(r, rad, rad, strokePaint);
        drawCenteredText(canvas, label, r.centerX(), r.centerY());
    }

    private void drawStick(Canvas canvas, Stick s) {
        canvas.drawCircle(s.cx, s.cy, s.baseR, fillPaint);
        canvas.drawCircle(s.cx, s.cy, s.baseR, strokePaint);
        float kx = s.cx + s.normX * (s.baseR - s.knobR * 0.4f);
        float ky = s.cy + s.normY * (s.baseR - s.knobR * 0.4f);
        canvas.drawCircle(kx, ky, s.knobR, s.pointerId != -1 ? activeFillPaint : fillPaint);
        canvas.drawCircle(kx, ky, s.knobR, strokePaint);
        drawCenteredText(canvas, s.label, kx, ky);
    }

    private void drawCenteredText(Canvas canvas, String text, float cx, float cy) {
        Paint.FontMetrics fm = textPaint.getFontMetrics();
        canvas.drawText(text, cx, cy - (fm.ascent + fm.descent) * 0.5f, textPaint);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        int idx = event.getActionIndex();
        int pid = event.getPointerId(idx);
        boolean consumed = false;

        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                float x = event.getX(idx), y = event.getY(idx);
                if (toggleRect.contains(x, y)) {
                    togglePointerId = pid;
                    consumed = true;
                    break;
                }
                if (!controlsVisible) return false;
                if (hitStick(leftStick, pid, x, y) || hitStick(rightStick, pid, x, y)) {
                    consumed = true;
                } else {
                    Control c = findControl(x, y);
                    if (c != null) {
                        pointerToControl.put(pid, c);
                        if (c.activePointers++ == 0 && c.code > 0) {
                            listener.onButtonChanged(c.code, true);
                        }
                        consumed = true;
                    }
                }
                break;
            }
            case MotionEvent.ACTION_MOVE: {
                if (!controlsVisible) return togglePointerId != -1;
                for (int i = 0; i < event.getPointerCount(); i++) {
                    int id = event.getPointerId(i);
                    float x = event.getX(i), y = event.getY(i);
                    if (id == leftStick.pointerId) {
                        updateStick(leftStick, x, y);
                        consumed = true;
                    } else if (id == rightStick.pointerId) {
                        updateStick(rightStick, x, y);
                        consumed = true;
                    } else if (pointerToControl.containsKey(id)) {
                        consumed = true;
                    }
                }
                break;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_CANCEL: {
                if (pid == togglePointerId) {
                    togglePointerId = -1;
                    if (action != MotionEvent.ACTION_CANCEL &&
                            toggleRect.contains(event.getX(idx), event.getY(idx))) {
                        controlsVisible = !controlsVisible;
                        if (!controlsVisible) releaseAll();
                    }
                    consumed = true;
                    break;
                }
                if (pid == leftStick.pointerId) {
                    leftStick.pointerId = -1;
                    leftStick.normX = 0f;
                    leftStick.normY = 0f;
                    consumed = true;
                }
                if (pid == rightStick.pointerId) {
                    rightStick.pointerId = -1;
                    rightStick.normX = 0f;
                    rightStick.normY = 0f;
                    consumed = true;
                }
                Control c = pointerToControl.remove(pid);
                if (c != null) {
                    if (--c.activePointers <= 0) {
                        c.activePointers = 0;
                        if (c.code > 0) listener.onButtonChanged(c.code, false);
                    }
                    consumed = true;
                }
                break;
            }
        }

        if (consumed) {
            dispatchAxesIfChanged();
            invalidate();
        }
        return consumed || togglePointerId != -1;
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        if (!hasWindowFocus) {
            togglePointerId = -1;
            releaseAll();
            invalidate();
        }
    }

    private void releaseAll() {
        leftStick.pointerId = -1;
        leftStick.normX = 0f;
        leftStick.normY = 0f;
        rightStick.pointerId = -1;
        rightStick.normX = 0f;
        rightStick.normY = 0f;
        for (Control c : controls) {
            if (c.activePointers > 0 && c.code > 0) listener.onButtonChanged(c.code, false);
            c.activePointers = 0;
        }
        pointerToControl.clear();
        dispatchAxesIfChanged();
    }

    private boolean hitStick(Stick s, int pid, float x, float y) {
        if (s.pointerId != -1) return false;
        float dx = x - s.cx, dy = y - s.cy;
        if (dx * dx + dy * dy <= s.baseR * s.baseR * 1.25f) {
            s.pointerId = pid;
            updateStick(s, x, y);
            return true;
        }
        return false;
    }

    private void updateStick(Stick s, float x, float y) {
        float dx = x - s.cx, dy = y - s.cy;
        float dist = (float) Math.hypot(dx, dy);
        if (dist > s.baseR && dist > 0f) {
            dx = dx * s.baseR / dist;
            dy = dy * s.baseR / dist;
        }
        s.normX = dx / s.baseR;
        s.normY = dy / s.baseR;
    }

    private Control findControl(float x, float y) {
        for (Control c : controls) {
            if (c.rect.contains(x, y)) return c;
        }
        return null;
    }

    private boolean isControlActive(int code) {
        for (Control c : controls) {
            if (c.code == code) return c.activePointers > 0;
        }
        return false;
    }

    private void dispatchAxesIfChanged() {
        int lt = isControlActive(TRIG_LT) ? 255 : 0;
        int rt = isControlActive(TRIG_RT) ? 255 : 0;
        int lx = Math.round(leftStick.normX * 32767f);
        int ly = Math.round(-leftStick.normY * 32767f);
        int rx = Math.round(rightStick.normX * 32767f);
        int ry = Math.round(-rightStick.normY * 32767f);
        if (lt != lastLt || rt != lastRt || lx != lastLx || ly != lastLy || rx != lastRx || ry != lastRy) {
            lastLt = lt; lastRt = rt; lastLx = lx; lastLy = ly; lastRx = rx; lastRy = ry;
            listener.onAxesChanged(lt, rt, lx, ly, rx, ry);
        }
    }
}
