#ifndef WSM_RENDER_INSPECTION_H
#define WSM_RENDER_INSPECTION_H

// Value-only, bounded developer overrides. Never retain a Pane/Layout pointer
// or mutate authored visibility/animation flags. All effects require a scope.
namespace RenderInspection
{
	enum Context { Grid, Banner, Settings, Board, Native, Home };
	enum Policy { Auto, Hidden, Shown };
	enum Kind { Container, Image, Text, Button, Panel, Channel, Mask, KindCount };
	const unsigned Capacity = 512;
	struct Element {
		char name[17]; // Internal identity only, not a user-facing label.
		Kind kind;
		char caption[97];
		float width,height;
		float left,top,right,bottom; // Normalized framebuffer crop; copied values.
		bool previewVisible;
	};
	void BeginFrame(Context context, unsigned key);
	void EndFrame();
	bool Active();
	bool Unmasked();
	bool MasksEnabled();
	void ToggleMasks();
	Policy Observe(const char *name, Kind kind=Container, float width=0, float height=0,
		const short *caption=0);
	Kind Classify(unsigned magic, const char *name, bool channel=false);
	const char *KindLabel(Kind kind);
	void RequestBounds(bool enabled);
	bool WantsBounds();
	void SetBounds(const char *name, float left, float top, float right, float bottom);
	bool IsMask(const char *name);
	unsigned Count();
	Element ElementAt(unsigned index);
	Context CurrentContext();
	unsigned CurrentKey();
	Policy GetPolicy(Context context, unsigned key, const char *name);
	void CyclePolicy(Context context, unsigned key, const char *name);
	void ToggleHidden(Context context, unsigned key, const char *name);
	void SetHidden(Context context, unsigned key, const char *name, bool hidden);
	void RestoreAll();
	// Inspection views affect copied submissions only; authored UI stays intact.
	void Configure3D(bool enabled,float yaw,float pitch,bool layers);
	void ConfigureSpectator(bool enabled,float yaw,float pitch,bool layers,float x,float y,float z);
	bool PerspectiveProjection(float (&out)[4][4],const float authored[4][4],
		const float (&reference)[4][4]);
	bool ThreeDActive();
	void TransformView(float (&view)[3][4],const float (&projection)[4][4]);
	class DrawLayerScope {
	public:
		explicit DrawLayerScope(const char *name);
		~DrawLayerScope();
	private:
		float previous;
	};
}
#endif
