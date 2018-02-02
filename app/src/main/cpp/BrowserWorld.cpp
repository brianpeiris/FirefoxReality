#include "BrowserWorld.h"
#include "vrb/CameraSimple.h"
#include "vrb/Color.h"
#include "vrb/ConcreteClass.h"
#include "vrb/Context.h"
#include "vrb/CullVisitor.h"
#include "vrb/DrawableList.h"
#include "vrb/Geometry.h"
#include "vrb/Group.h"
#include "vrb/Light.h"
#include "vrb/Logger.h"
#include "vrb/Matrix.h"
#include "vrb/NodeFactoryObj.h"
#include "vrb/ParserObj.h"
#include "vrb/RenderState.h"
#include "vrb/SurfaceTextureFactory.h"
#include "vrb/TextureSurface.h"
#include "vrb/Transform.h"
#include "vrb/VertexArray.h"
#include "vrb/Vector.h"

using namespace vrb;

namespace {

static const char *kSetSurfaceTextureName = "setSurfaceTexture";
static const char *kSetSurfaceTextureSignature = "(Ljava/lang/String;Landroid/graphics/SurfaceTexture;II)V";
class SurfaceObserver;
typedef std::shared_ptr<SurfaceObserver> SurfaceObserverPtr;

class SurfaceObserver : public SurfaceTextureObserver {
public:
  SurfaceObserver(BrowserWorldWeakPtr& aWorld);
  ~SurfaceObserver();
  void SurfaceTextureCreated(const std::string& aName, GLuint aHandle, jobject aSurfaceTexture) override;
  void SurfaceTextureHandleUpdated(const std::string aName, GLuint aHandle) override;
  void SurfaceTextureDestroyed(const std::string& aName) override;
  void SurfaceTextureCreationError(const std::string& aName, const std::string& aReason) override;

protected:
  BrowserWorldWeakPtr mWorld;
};

SurfaceObserver::SurfaceObserver(BrowserWorldWeakPtr& aWorld) : mWorld(aWorld) {}
SurfaceObserver::~SurfaceObserver() {}

void
SurfaceObserver::SurfaceTextureCreated(const std::string& aName, GLuint aHandle, jobject aSurfaceTexture) {
  BrowserWorldPtr world = mWorld.lock();
  if (world) {
    world->SetSurfaceTexture(aName, aSurfaceTexture);
  }
}

void
SurfaceObserver::SurfaceTextureHandleUpdated(const std::string aName, GLuint aHandle) {}

void
SurfaceObserver::SurfaceTextureDestroyed(const std::string& aName) {
  BrowserWorldPtr world = mWorld.lock();
  if (world) {
    jobject nullObject = nullptr;
    world->SetSurfaceTexture(aName, nullObject);
  }
}

void
SurfaceObserver::SurfaceTextureCreationError(const std::string& aName, const std::string& aReason) {
  
}


} // namespace


struct BrowserWorld::State {
  BrowserWorldWeakPtr self;
  SurfaceObserverPtr surfaceObserver;
  bool paused;
  bool glInitialized;
  float heading;
  ContextPtr context;
  ContextWeak contextWeak;
  NodeFactoryObjPtr factory;
  ParserObjPtr parser;
  GroupPtr root;
  LightPtr light;
  TransformPtr browser;
  TransformPtr controller;
  TextureSurfacePtr browserSurface;
  CullVisitorPtr cullVisitor;
  DrawableListPtr drawList;
  CameraSimplePtr camera;
  JNIEnv* env;
  jobject activity;
  jmethodID setSurfaceTextureMethod;


  State() : paused(true), glInitialized(false), heading(0.0f), env(nullptr), activity(nullptr), setSurfaceTextureMethod(nullptr) {
    context = Context::Create();
    contextWeak = context;
    factory = NodeFactoryObj::Create(contextWeak);
    parser = ParserObj::Create(contextWeak);
    parser->SetObserver(factory);
    root = Group::Create(contextWeak);
    light = Light::Create(contextWeak);
    root->AddLight(light);
    browserSurface = TextureSurface::Create(contextWeak, "browser");
    cullVisitor = CullVisitor::Create(contextWeak);
    drawList = DrawableList::Create(contextWeak);
    camera = CameraSimple::Create(contextWeak);
  }
};


BrowserWorldPtr
BrowserWorld::Create() {
  BrowserWorldPtr result = std::make_shared<vrb::ConcreteClass<BrowserWorld, BrowserWorld::State> >();
  result->m.self = result;
  result->m.surfaceObserver = std::make_shared<SurfaceObserver>(result->m.self);
  result->m.context->GetSurfaceTextureFactory()->AddGlobalObserver(result->m.surfaceObserver);
  return result;
}

vrb::ContextPtr
BrowserWorld::GetContext() {
  return m.context;
}

void
BrowserWorld::Pause() {
  m.paused = true;
}

void
BrowserWorld::Resume() {
  m.paused = false;
}

void
BrowserWorld::SetViewport(const float aWidth, const float aHeight) {
  m.camera->SetViewport(aWidth, aHeight);
  const bool wider = (aWidth > aHeight);
  m.camera->SetFieldOfView((wider ? 60.0f : -1.0f), (wider ? -1.0f : 60.0f));
  m.camera->SetTransform(Matrix::Position(Vector(0.0f, 0.0f, 16.0f)));
}

void
BrowserWorld::InitializeJava(JNIEnv* aEnv, jobject& aActivity, jobject& aAssetManager) {
  if (m.context) {
    m.context->InitializeJava(aEnv, aAssetManager);
  }
  m.env = aEnv;
  if (!m.env) {
    return;
  }
  m.activity = m.env->NewGlobalRef(aActivity);
  if (!m.activity) {
    return;
  }
  jclass clazz = m.env->GetObjectClass(m.activity);
  if (!clazz) {
    return;
  }
  m.setSurfaceTextureMethod = m.env->GetMethodID(clazz, kSetSurfaceTextureName, kSetSurfaceTextureSignature);
  if (!m.setSurfaceTextureMethod) {
    VRB_LOG("Failed to find Java method: %s %s", kSetSurfaceTextureName, kSetSurfaceTextureSignature);
  }
}

void
BrowserWorld::InitializeGL() {
VRB_LINE;
  if (m.context) {
    if (!m.browser) {
      CreateBrowser();
    }
    if (!m.controller) {
      m.controller = Transform::Create(m.contextWeak);
      m.factory->SetModelRoot(m.controller);
      m.parser->LoadModel("vr_controller_daydream.obj");
      //m.parser->LoadModel("teapot.obj");
      //m.parser->LoadModel("daydream.obj");
      m.root->AddNode(m.controller);
      m.controller->SetTransform(Matrix::Position(Vector(0.0f, 0.0f, 15.5f)));
    }
    m.glInitialized = m.context->InitializeGL();
  }
}

void
BrowserWorld::Shutdown() {
  if (m.context) {
    m.context->Shutdown();
    m.glInitialized = false;
  }
  if (m.env) {
    m.env->DeleteGlobalRef(m.activity);
  }
  m.activity = nullptr;
  m.setSurfaceTextureMethod = nullptr;
  m.paused = true;
}

void
BrowserWorld::Draw() {
  if (m.paused) {
    return;
  }
  if (!m.glInitialized) {
    m.glInitialized = m.context->InitializeGL();
    if (!m.glInitialized) {
      return;
    }
  }
  m.context->Update();
  //m.browser->SetTransform(vrb::Matrix::Rotation(vrb::Vector(1.0f, 0.0f, 0.0f), M_PI * 0.5f));
  m.controller->SetTransform(Matrix::Position(Vector(0.0f, 0.0f, 15.5f)).PostMultiply(Matrix::Rotation(Vector(0.0f, 0.0f, 1.0f), m.heading).PreMultiply(Matrix::Rotation(Vector(0.0f, 1.0f, 0.0f), m.heading))));
  m.browser->SetTransform(Matrix::Rotation(Vector(0.0f, 1.0f, 0.0f), m.heading));
  m.drawList->Reset();
  m.root->Cull(*m.cullVisitor, *m.drawList);
  m.drawList->Draw(*m.camera);
  m.heading += M_PI / 120.0f;
  if (m.heading > (2.0f * M_PI)) { m.heading = 0.0f; }
}

void
BrowserWorld::SetSurfaceTexture(const std::string& aName, jobject& aSurface) {
  if (m.env && m.activity && m.setSurfaceTextureMethod) {
    jstring name = m.env->NewStringUTF(aName.c_str());
    m.env->CallVoidMethod(m.activity, m.setSurfaceTextureMethod, name, aSurface, 1024, 1024);
    m.env->DeleteLocalRef(name);
  }
}

void
BrowserWorld::CreateBrowser() {
  VertexArrayPtr array = VertexArray::Create(m.contextWeak);
  const float kLength = 5.0f;
  array->AppendVertex(Vector(-kLength, -kLength, 0.0f)); // Bottom left
  array->AppendVertex(Vector(kLength, -kLength, 0.0f)); // Bottom right
  array->AppendVertex(Vector(kLength, kLength, 0.0f)); // Top right
  array->AppendVertex(Vector(-kLength, kLength, 0.0f)); // Top left

  array->AppendUV(Vector(0.0f, 1.0f, 0.0f));
  array->AppendUV(Vector(1.0f, 1.0f, 0.0f));
  array->AppendUV(Vector(1.0f, 0.0f, 0.0f));
  array->AppendUV(Vector(0.0f, 0.0f, 0.0f));

  const Vector kNormal(0.0f, 0.0f, 1.0f); // out of the screen
  array->AppendNormal(kNormal);

  RenderStatePtr state = RenderState::Create(m.contextWeak);
  state->SetTexture(m.browserSurface);
  state->SetMaterial(Color(0.4f, 0.4f, 0.4f), Color(1.0f, 1.0f, 1.0f), Color(0.0f, 0.0f, 0.0f), 0.0f);
  GeometryPtr geometry = Geometry::Create(m.contextWeak);
  geometry->SetVertexArray(array);
  geometry->SetRenderState(state);

  std::vector<int> index;
  index.push_back(1);
  index.push_back(2);
  index.push_back(3);
  index.push_back(4);
  std::vector<int> normalIndex;
  normalIndex.push_back(1);
  normalIndex.push_back(1);
  normalIndex.push_back(1);
  normalIndex.push_back(1);
  geometry->AddFace(index, index, normalIndex);

  // Draw the back for now
  index.clear();
  index.push_back(1);
  index.push_back(4);
  index.push_back(3);
  index.push_back(2);

  array->AppendNormal(-kNormal);
  normalIndex.clear();
  normalIndex.push_back(2);
  normalIndex.push_back(2);
  normalIndex.push_back(2);
  normalIndex.push_back(2);
  geometry->AddFace(index, index, normalIndex);

  m.browser = Transform::Create(m.contextWeak);
  m.browser->AddNode(geometry);
  m.root->AddNode(m.browser);
}

BrowserWorld::BrowserWorld(State& aState) : m(aState) {}

BrowserWorld::~BrowserWorld() {}